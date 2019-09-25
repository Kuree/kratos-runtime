#include "control.hh"
#include <unistd.h>
#include <filesystem>
#include <iostream>
#include <memory>
#include <mutex>
#include <unordered_map>
#include "db.hh"
#include "fmt/format.h"
#include "httplib.h"
#include "json11/json11.hpp"
#include "sim.hh"
#include "std/vpi_user.h"
#include "util.hh"

// constants
constexpr uint16_t runtime_port = 8888;

std::unique_ptr<httplib::Server> http_server = nullptr;
std::unique_ptr<httplib::Client> http_client = nullptr;
std::thread runtime_thread;
std::unordered_map<vpiHandle, std::string> monitored_signal_names;
std::unordered_map<std::string, vpiHandle> signal_call_back;
std::unique_ptr<Database> db_;
// include the dot to make things easier
std::string top_name_ = "TOP.";  // NOLINT
// mutex
std::mutex runtime_lock;
// step over. notice that this is not mutex protected. You should not set step over during
// the simulation
bool step_over = false;

std::optional<std::string> get_value(std::string handle_name);
std::optional<std::string> get_simulation_time(const std::string &);


std::string get_breakpoint_value(uint32_t id) {
    std::vector<std::pair<std::string, std::string>> self_vars;
    std::vector<std::pair<std::string, std::string>> gen_vars;
    std::vector<std::pair<std::string, std::string>> local_vars;
    if (db_) {
        auto variables = db_->get_variable_mapping(id);
        for (auto const &variable : variables) {
            // decide if we need to append the top name
            if (variable.is_var) {
                auto value = get_value(fmt::format("{0}.{1}", variable.handle, variable.var));
                std::string v;
                if (value)
                    v = value.value();
                else
                    v = "ERROR";
                if (variable.front_var.empty()) {
                    gen_vars.emplace_back(std::make_pair(variable.var, v));
                } else {
                    self_vars.emplace_back(std::make_pair(variable.front_var, v));
                }
            } else {
                self_vars.emplace_back(variable.front_var, variable.var);
            }

        }
        auto context_vars = db_->get_context_variable(id);
        for (auto const &variable: context_vars) {
            if (variable.is_var) {
                auto value = get_value(variable.value);
                std::string v;
                if (value)
                    v = value.value();
                else
                    v = "ERROR";
                local_vars.emplace_back(std::make_pair(variable.name, v));
            } else {
                local_vars.emplace_back(std::make_pair(variable.name, variable.value));
            }
        }
    }

    // send over the line number and filename as well
    std::string filename;
    std::string line_num;
    if (db_) {
        auto bp = db_->get_breakpoint_info(id);
        if (bp) {
            filename = bp.value().first;
            line_num = fmt::format("{0}", bp.value().second);
        }
    }
    json11::Json result = json11::Json::object({{"id", fmt::format("{0}", id)},
                                                {"self", self_vars},
                                                {"local", local_vars},
                                                {"generator", gen_vars},
                                                {"filename", filename},
                                                {"line_num", line_num}});
    return result.dump();
}

void breakpoint_trace(uint32_t id) {
    if (step_over || !should_continue_simulation(id)) {
        printf("hit breakpoint %d\n", id);
        // tell the client that we have hit a clock
        if (http_client) {
            auto content = get_breakpoint_value(id);
            if (step_over) {
                http_client->Post("/status/step", content, "application/json");
            } else {
                http_client->Post("/status/breakpoint", content, "application/json");
            }
        }
        // hold the lock
        runtime_lock.lock();
    }
}

std::vector<uint32_t> get_breakpoint(const std::string &body, httplib::Response &res) {
    std::string error;
    auto json = json11::Json::parse(body, error);
    auto filename = json["filename"];
    auto line_num = json["line_num"];
    if (error.empty() && !filename.is_null() && !line_num.is_null()) {
        auto const &filename_str = filename.string_value();
        auto line_num_int = line_num.int_value();
        // hacky way
        if (!db_) {
            while (!db_) {
                // sleep for 0.1 seconds
                usleep(100000);
            }
        }
        if (db_) {
            auto bps = db_->get_breakpoint_id(filename_str, line_num_int);
            if (!bps.empty()) {
                res.status = 200;
                res.set_content("Okay", "text/plain");
                return bps;
            }
        }
    }
    if (error.empty()) {
        error = "ERROR";
    }
    res.status = 401;
    res.set_content(error, "text/plain");
    return {};
}

std::vector<uint32_t> get_breakpoint_filename(const std::string &filename, httplib::Response &res) {
    if (!filename.empty()) {
        if (db_) {
            res.status = 200;
            // return a list of breakpoints
            res.set_content("Okay", "text/plain");
            auto bps = db_->get_all_breakpoints(filename);
            struct BP {
                uint32_t id;
                [[nodiscard]] std::string to_json() const { return fmt::format("{0}", id); }
            };
            std::vector<BP> json_bp;
            json_bp.reserve(bps.size());
            for (auto const &bp : bps) {
                json_bp.emplace_back(BP{bp});
            }
            std::string value = json11::Json(json_bp).dump();
            return bps;
        }
    }
    res.status = 401;
    res.set_content("[]", "text/plain");
    return {};
}

std::optional<std::string> get_value(std::string handle_name) {
    if (handle_name == "time") {
        return get_simulation_time("");
    }
    // change the handle name
    if (handle_name.size() < top_name_.size() ||
        handle_name.substr(top_name_.size()) != top_name_) {
        std::string format;
        if (top_name_.back() == '.')
            format = "{0}{1}";
        else
            format = "{0}.{1}";
        handle_name = fmt::format(format, top_name_, handle_name);
    }

    auto handle = const_cast<char *>(handle_name.c_str());
    vpiHandle vh = vpi_handle_by_name(handle, nullptr);
    if (!vh) {
        // not found
        return std::nullopt;
    } else {
        s_vpi_value v;
        v.format = vpiIntVal;
        vpi_get_value(vh, &v);
        int64_t result = v.value.integer;
        return fmt::format("{0}", result);
    }
}

std::optional<std::string> get_simulation_time(const std::string &module_name = "") {
    s_vpi_time current_time;
    // verilator only supports vpiSimTime
    current_time.type = vpiSimTime;
    current_time.real = 0;
    if (module_name.empty()) {
        vpi_get_time(nullptr, &current_time);
        uint64_t high = current_time.high;
        uint32_t low = current_time.low;
        return fmt::format("{0}", high << 32u | low);
    } else {
        auto handle = const_cast<char *>(module_name.c_str());
        vpiHandle module_handle = vpi_handle_by_name(handle, nullptr);
        if (module_handle) {
            uint64_t high = current_time.high;
            uint32_t low = current_time.low;
            return fmt::format("{0}", high << 32u | low);
        }
    }
    return std::nullopt;
}

int monitor_signal(p_cb_data cb_data_p) {
    std::string signal_name = cb_data_p->user_data;
    std::string value = cb_data_p->value->value.str;
    if (http_client) {
        http_client->Post(fmt::format("/value/{0}", signal_name).c_str(), value, "text/plain");
    }
    return 0;
}

bool setup_monitor(const std::string &signal_name) {
    // get the handle
    auto handle = const_cast<char *>(signal_name.c_str());
    vpiHandle vh = vpi_handle_by_name(handle, nullptr);
    if (!vh) {
        // not found
        return false;
    } else {
        if (signal_call_back.find(signal_name) != signal_call_back.end()) return true;
        s_vpi_time time_s = {vpiSimTime};
        s_vpi_value value_s = {vpiIntVal};

        s_cb_data cb_data_s = {cbValueChange, monitor_signal, nullptr, &time_s, &value_s};
        cb_data_s.obj = vh;
        // use the global allocated string to avoid memory leak
        monitored_signal_names[vh] = signal_name;
        auto &str_p = monitored_signal_names.at(vh);
        auto char_p = const_cast<char *>(str_p.c_str());
        cb_data_s.user_data = char_p;
        auto r = vpi_register_cb(&cb_data_s);
        signal_call_back.emplace(signal_name, r);
        return true;
    }
}

bool remove_monitor(const std::string &signal_name) {
    if (signal_call_back.find(signal_name) == signal_call_back.end()) return false;
    auto cb_vh = signal_call_back.at(signal_name);
    return vpi_remove_cb(cb_vh) == 1;
}

void initialize_runtime() {
    using namespace httplib;
    http_server = std::make_unique<Server>();

    // setup call backs
    http_server->Post(R"(/breakpoint/(\d+))", [](const Request &req, Response &res) {
        auto num = req.matches[1];
        try {
            auto id = std::stoi(num);
            add_break_point(id);
            printf("Breakpoint inserted to %d\n", id);
            res.status = 200;
            res.set_content("Okay", "text/plain");
            return;
        } catch (...) {
            res.status = 401;
            res.set_content("ERROR", "text/plain");
            return;
        }
    });

    http_server->Get("/breakpoint", [](const Request &req, Response &res) {
        auto bp = get_breakpoint(req.body, res);
    });

    http_server->Post("/breakpoint", [](const Request &req, Response &res) {
        auto bps = get_breakpoint(req.body, res);
        if (!bps.empty()) {
            for (auto const &bp: bps) {
                add_break_point(bp);
                printf("Breakpoint inserted to %d\n", bp);
            }
        }
    });

    http_server->Delete("/breakpoint", [](const Request &req, Response &res) {
        auto bps = get_breakpoint(req.body, res);
        if (db_ && !bps.empty()) {
            for (auto const &bp: bps) {
                remove_break_point(bp);
                printf("Breakpoint removed from %d\n", bp);
                return;
            }
        } else {
            res.status = 401;
            res.set_content("ERROR", "text/plain");
        }
    });

    http_server->Delete(R"(/breakpoint/(\d+))", [](const Request &req, Response &res) {
        auto num = req.matches[1];
        try {
            auto id = std::stoi(num);
            remove_break_point(id);
            printf("Breakpoint removed from %d\n", id);
            res.status = 200;
            res.set_content("Okay", "text/plain");
            return;
        } catch (...) {
            res.status = 401;
            res.set_content("ERROR", "text/plain");
            return;
        }
    });

    // delete all breakpoint from a file
    http_server->Delete(R"(/breakpoint/file/([\w./:\\]+))", [](const Request &req, Response &res) {
        auto filename = req.matches[1];
        auto bps = get_breakpoint_filename(filename, res);
        for (auto const &bp : bps) {
            printf("Breakpoint removed from %d\n", bp);
            remove_break_point(bp);
        }
    });

    // get all the files
    http_server->Get("/files", [](const Request &req, Response &res) {
        if (db_) {
            auto names = db_->get_all_files();
            struct StrValue {
                std::string value;
                [[nodiscard]] std::string to_json() const { return value; }
            };
            std::vector<StrValue> values;
            values.reserve(names.size());
            for (auto const &name : names) {
                values.emplace_back(StrValue{name});
            }
            auto content = json11::Json(values).dump();
            res.set_content(content, "application/json");
            res.status = 200;
        } else {
            res.set_content("ERROR", "text/plain");
            res.status = 401;
        }
    });

    http_server->Get(R"(/value/([\w.$]+))", [](const Request &req, Response &res) {
        auto name = req.matches[1];
        auto result = get_value(name);
        if (result) {
            res.status = 200;
            res.set_content(result.value(), "text/plain");
        } else {
            res.status = 401;
            res.set_content("ERROR", "text/plain");
        }
    });

    http_server->Get("/values", [](const Request &req, Response &res) {
        std::string err;
        auto json = json11::Json::parse(req.body, err);
        if (err.empty()) {
            auto const &lst = json.array_items();
            struct Entry {
                std::string name;
                std::string value;
                [[nodiscard]] json11::Json to_json() const {
                    return json11::Json::object{{{"name", name}, {"value", value}}};
                }
            };
            std::vector<Entry> result;
            for (auto const &entry : lst) {
                auto const &name = entry.string_value();
                auto v = get_value(name);
                std::string value;
                if (v) {
                    value = v.value();

                } else {
                    value = "ERROR";
                }
                result.emplace_back(Entry{name, value});
            }
            res.status = 200;
            auto content = json11::Json(result).dump();
            res.set_content(content, "application/text");
        } else {
            res.status = 401;
            res.set_content("[]", "application/json");
        }
    });

    http_server->Get("/time", [](const Request &req, Response &res) {
        auto time = get_simulation_time("");
        if (time) {
            res.status = 200;
            res.set_content(fmt::format("{0}", time.value()), "text/plain");
        } else {
            res.status = 401;
            res.set_content("ERROR", "text/plain");
        }
    });

    http_server->Post(R"(/monitor/([\w.$]+))", [](const Request &req, Response &res) {
        auto name = req.matches[1];
        auto result = setup_monitor(name);
        if (result) {
            res.status = 200;
            res.set_content("Okay", "text/plain");
        } else {
            res.status = 401;
            res.set_content("ERROR", "text/plain");
        }
    });

    http_server->Delete(R"(/monitor/([\w.$]+))", [](const Request &req, Response &res) {
        auto name = req.matches[1];
        auto result = remove_monitor(name);
        if (result) {
            res.status = 200;
            res.set_content("Okay", "text/plain");
        } else {
            res.status = 401;
            res.set_content("ERROR", "text/plain");
        }
    });

    http_server->Post("/continue", [](const Request &req, Response &res) {
        step_over = false;
        runtime_lock.unlock();
        res.status = 200;
        res.set_content("Okay", "text/plain");
    });

    http_server->Post("/step_over", [](const Request &req, Response &res) {
        step_over = true;
        runtime_lock.unlock();
        res.status = 200;
        res.set_content("Okay", "text/plain");
    });

    http_server->Post("/connect", [](const Request &req, Response &res) {
        // parse the content
        auto const &body = req.body;
        std::string err;
        auto payload = json11::Json::parse(body, err);
        auto ip_json = payload["ip"];
        auto port_json = payload["port"];
        auto db_json = payload["database"];
        bool has_error = false;
        if (port_json.is_null() || ip_json.is_null() || db_json.is_null() ||
            !port_json.is_number() || !ip_json.is_string() || !db_json.is_string()) {
            has_error = true;
        }
        if (!has_error) {
            auto const &ip = ip_json.string_value();
            auto port = static_cast<int>(port_json.number_value());
            auto &db_filename = db_json.string_value();

            if (!std::filesystem::exists(db_filename)) {
                has_error = true;
            } else {
                try {
                    http_client = std::make_unique<Client>(ip.c_str(), port);
                    // load up the database
                    db_ = std::make_unique<Database>(db_filename);
                    printf("Debugger connected to %d\n", port);
                } catch (...) {
                    http_client = nullptr;
                    db_ = nullptr;
                    has_error = true;
                }
            }
        }
        if (!has_error) {
            res.status = 200;
            res.set_content("Okay", "text/plain");
        } else {
            res.status = 401;
            res.set_content("ERROR", "text/plain");
        }
    });

    // start the http in a different thread
    runtime_thread = std::thread([=]() { http_server->listen("0.0.0.0", runtime_port); });

    // by default it's locked
    runtime_lock.lock();
    runtime_lock.lock();
}

PLI_INT32 initialize_server_vpi(s_cb_data *) {
    initialize_runtime();
    return 0;
}

void teardown_runtime() {
    // send stop signal to the debugger
    if (http_client) {
        http_client->Post("/stop", "", "text/plain");
    }
    runtime_lock.unlock();
    http_server->stop();
    // this may take some time due to system resource allocation
    runtime_thread.join();
}

PLI_INT32 teardown_server_vpi(s_cb_data *) {
    teardown_runtime();
    return 0;
}

// verilator doesn't like this kind of registration
void initialize_runtime_vpi() {
    // register the initialize server when the simulation started
    s_cb_data cb_data_init;
    cb_data_init.reason = cbStartOfSimulation;
    cb_data_init.cb_rtn = &initialize_server_vpi;
    cb_data_init.obj = nullptr;
    cb_data_init.index = 0;
    cb_data_init.value = nullptr;
    cb_data_init.time = nullptr;
    vpiHandle res = vpi_register_cb(&cb_data_init);
    if (!res) {
        std::cerr << "ERROR: failed to register runtime initialization" << std::endl;
    }
    // clear the handle since we don't need it
    vpi_free_object(res);

    // register the initialize server when the simulation started
    s_cb_data cb_data_teardown;
    cb_data_teardown.reason = cbEndOfSimulation;
    cb_data_teardown.cb_rtn = &teardown_server_vpi;
    cb_data_teardown.obj = nullptr;
    cb_data_teardown.index = 0;
    cb_data_teardown.value = nullptr;
    cb_data_teardown.time = nullptr;
    res = vpi_register_cb(&cb_data_teardown);
    if (!res) {
        std::cerr << "ERROR: failed to register runtime initialization" << std::endl;
    }
    vpi_free_object(res);
}

extern "C" {
// these are system level calls. register it to the simulator
void (*vlog_startup_routines[])() = {initialize_runtime_vpi, nullptr};
}
