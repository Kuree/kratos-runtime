#include "control.hh"
#include <unistd.h>
#include <filesystem>
#include <iostream>
#include <memory>
#include <mutex>
#include <unordered_map>
#include "db.hh"
#include "expr.hh"
#include "fmt/format.h"
#include "httplib.h"
#include "json11/json11.hpp"
#include "sim.hh"
#include "std/vpi_user.h"
#include "util.hh"

// constants
constexpr uint16_t runtime_port = 8888;
constexpr int BUFFER_SIZE = 1024;

std::unique_ptr<httplib::Server> http_server = nullptr;
std::unique_ptr<httplib::Client> http_client = nullptr;
std::thread runtime_thread;
std::unique_ptr<Database> db_;
std::unordered_map<uint32_t, std::unordered_map<std::string, std::string>>
    breakpoint_symbol_mapping;
// this is for vpi optimization
std::unordered_map<std::string, vpiHandle> vpi_handle_map;
// include the dot to make things easier
std::string top_name_ = "TOP.";  // NOLINT
// mutex
std::mutex runtime_lock;
std::mutex vpi_lock;
// step over. notice that this is not mutex protected. You should not set step over during
// the simulation
bool step_over = false;
// whether to pause at the the clock edge
bool pause_clock_edge = false;
// current scope it has to be set to get the connections
std::string current_scope;
// we don't want to send back the values if it's never stopped
bool has_paused_on_clock = false;

std::optional<std::string> get_value(std::string handle_name);
std::optional<std::string> get_simulation_time(const std::string &);
bool evaluate_breakpoint_expr(uint32_t breakpoint_id);

struct CbHandle {
    s_vpi_time time;
    s_vpi_value value;
    s_cb_data cb_data;
    vpiHandle cb_handle = nullptr;
    char *name = nullptr;
};

// this is for vpi cb struct
std::unordered_map<std::string, CbHandle *> cb_handle_map;

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
        for (auto const &variable : context_vars) {
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
        // if we have a conditional breakpoint
        // we need to check that
        if (has_expr_breakpoint(id)) {
            if (!evaluate_breakpoint_expr(id)) return;
        }
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

json11::Json::object get_graph_value() {
    auto time_val = get_simulation_time("");
    std::string time = "ERROR";
    if (time_val) time = *time_val;
    // we need to pull out all the values from the connections
    auto modules = db_->get_hierarchy(current_scope);
    std::map<std::string, std::string> values;
    for (auto const &module : modules) {
        auto handle = fmt::format("{0}.{1}", module.parent_handle, module.child);
        auto conn_from = db_->get_connection_from(handle);
        auto conn_to = db_->get_connection_to(handle);
        for (auto &conn : conn_from) {
            auto var_from_handle = fmt::format("{0}.{1}", conn.handle_from, conn.var_from);
            auto var_to_handle = fmt::format("{0}.{1}", conn.handle_to, conn.var_to);
            auto value = get_value(var_from_handle);
            if (value) {
                // from and to are the same
                values.emplace(var_from_handle, *value);
                values.emplace(var_to_handle, *value);
            }
        }
    }
    return json11::Json::object({{"time", time}, {"value", values}});
}

void breakpoint_clock(void) {
    if (pause_clock_edge) {
        has_paused_on_clock = true;
        printf("Pause on clock edge\n");
        if (http_client && db_) {
            auto content = json11::Json(get_graph_value());
            http_client->Post("/status/clock",
                              content.dump(),  // NOLINT
                              "application/json");
            runtime_lock.lock();
        }
    }
}

std::vector<std::pair<uint32_t, std::string>> get_breakpoint(const std::string &body,
                                                             httplib::Response &res) {
    std::string error;
    auto json = json11::Json::parse(body, error);
    auto filename = json["filename"];
    auto line_num = json["line_num"];
    auto expression = json["expr"];
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
                std::vector<std::pair<uint32_t, std::string>> result;
                result.reserve(bps.size());
                std::string expr_str;
                if (!expression.is_null()) expr_str = expression.string_value();
                for (auto const &bp : bps) {
                    result.emplace_back(std::make_pair(bp, expr_str));
                }
                return result;
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

bool add_breakpoint_expr(uint32_t breakpoint_id, const std::string &expr) {
    if (expr.empty()) return true;
    if (!db_) return false;
    // query the local port variables
    auto const self_variables = db_->get_variable_mapping(breakpoint_id);
    auto const context_variables = db_->get_context_variable(breakpoint_id);
    std::unordered_map<std::string, int64_t> constants;
    std::unordered_set<std::string> symbols;
    // need to extract the time
    const static std::string time_var_name = "time";
    bool has_time_var_name_as_context = false;

    // initialize the mapping
    breakpoint_symbol_mapping[breakpoint_id] = {};
    // this is self variables
    for (auto const &v : self_variables) {
        auto front_var = v.front_var;
        // if front var is empty, it means it's generator variables
        if (front_var.empty()) continue;
        // hacky way to detect if the front var exists in the expression
        front_var = fmt::format("{0}.{1}", "self", front_var);
        if (is_expr_symbol(expr, front_var)) {
            if (v.is_var) {
                // compute handle name
                auto handle_name = fmt::format("{0}.{1}", v.handle, v.var);
                handle_name = get_handle_name(top_name_, handle_name);
                breakpoint_symbol_mapping[breakpoint_id].emplace(front_var, handle_name);
                symbols.emplace(front_var);
            } else {
                try {
                    auto value = std::stoi(v.var);
                    constants.emplace(front_var, value);
                } catch (...) {
                    return false;
                }
            }
        }
    }
    // need to compute local variables
    for (auto const &v : context_variables) {
        if (is_expr_symbol(expr, v.name)) {
            if (v.name == time_var_name) has_time_var_name_as_context = true;
            if (v.is_var) {
                auto handle_name = get_handle_name(top_name_, v.value);
                breakpoint_symbol_mapping[breakpoint_id].emplace(v.name, handle_name);
                symbols.emplace(v.name);
            } else {
                try {
                    auto value = std::stoi(v.value);
                    constants.emplace(v.name, value);
                } catch (...) {
                    return false;
                }
            }
        }
    }
    // special treatment for time
    const static std::string time_handle = "$time";
    const static std::string time_alias = "time_";
    const auto time = has_time_var_name_as_context ? time_alias : time_var_name;
    if (is_expr_symbol(expr, time)) {
        breakpoint_symbol_mapping[breakpoint_id].emplace(time, time_handle);
        symbols.emplace(time);
    }

    // add expression to the table
    try {
        add_expr(breakpoint_id, expr, symbols, constants);
        return true;
    } catch (...) {
        return false;
    }
}

bool evaluate_breakpoint_expr(uint32_t breakpoint_id) {
    // retrieve symbol values
    // we assume we already check if the system contains the breakpoint
    auto symbols = breakpoint_symbol_mapping.at(breakpoint_id);
    std::unordered_map<std::string, int64_t> values;
    for (auto const &[var, handle_name] : symbols) {
        auto v_str = get_value(handle_name);
        if (!v_str) {
            // unable to obtain the value
            // by default break
            return true;
        }
        auto value = std::stoi(*v_str);
        values.emplace(var, value);
    }
    return evaluate(breakpoint_id, values);
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
    if (handle_name == "time" || handle_name == "$time") {
        return get_simulation_time("");
    }
    handle_name = get_handle_name(top_name_, handle_name);
    vpiHandle vh;
    if (vpi_handle_map.find(handle_name) != vpi_handle_map.end()) {
        vh = vpi_handle_map.at(handle_name);
    } else {
        auto handle = const_cast<char *>(handle_name.c_str());
        vh = vpi_handle_by_name(handle, nullptr);
        vpi_handle_map.emplace(handle_name, vh);
    }

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
    std::string value = fmt::format("{0}", cb_data_p->value->value.integer);
    if (http_client) {
        printf("sending value %s\n", signal_name.c_str());
        auto json = json11::Json(json11::Json::object{{"handle", signal_name}, {"value", value}});
        http_client->Post("/value", json.dump(), "application/json");
    }
    return 0;
}

bool setup_monitor(std::string signal_name) {
    vpi_lock.lock();
    signal_name = get_handle_name(top_name_, signal_name);
    // get the handle
    vpiHandle vh;
    if (vpi_handle_map.find(signal_name) != vpi_handle_map.end()) {
        vh = vpi_handle_map.at(signal_name);
    } else {
        auto handle = const_cast<char *>(signal_name.c_str());
        vh = vpi_handle_by_name(handle, nullptr);
    }
    if (!vh) {
        // not found
        return false;
    } else {
        if (cb_handle_map.find(signal_name) != cb_handle_map.end()) return true;
        auto cb_handle = new CbHandle();
        cb_handle_map.emplace(signal_name, cb_handle);
        cb_handle->time = {vpiSimTime};
        cb_handle->value = {vpiIntVal};

        cb_handle->cb_data = {cbValueChange, monitor_signal, nullptr, &(cb_handle->time),
                              &(cb_handle->value)};
        cb_handle->cb_data.cb_rtn = monitor_signal;
        cb_handle->cb_data.obj = vh;
        cb_handle->name = (char *)calloc(0, signal_name.size() + 1);
        strncpy(cb_handle->name, signal_name.c_str(), signal_name.size());
        cb_handle->cb_data.user_data = cb_handle->name;

        auto r = vpi_register_cb(&cb_handle->cb_data);
        vpi_lock.unlock();

        cb_handle->cb_handle = r;

        printf("monitor added to %s\n", signal_name.c_str());

        return true;
    }
}

bool remove_monitor(std::string signal_name) {
    vpi_lock.lock();
    signal_name = get_handle_name(top_name_, signal_name);
    if (cb_handle_map.find(signal_name) == cb_handle_map.end()) return false;
    auto cb = cb_handle_map.at(signal_name);
    printf("monitor removed from %s\n", signal_name.c_str());

    auto r = vpi_remove_cb(cb->cb_handle);
    vpi_lock.unlock();

    free(cb->name);
    vpi_free_object(cb->cb_handle);
    delete cb;

    return r == 1;
}

void remove_all_monitor() {
    std::unordered_set<CbHandle *> handles;
    for (auto const &iter : cb_handle_map) {
        auto cb = iter.second;
        vpi_remove_cb(cb->cb_handle);
        handles.emplace(cb);
    }
    cb_handle_map.clear();
    for (auto cb : handles) {
        free(cb->name);
        vpi_free_object(cb->cb_handle);
        delete cb;
    }
    printf("monitors removed\n");
}

std::string get_connection_str(const std::string &handle_name, bool is_from) {
    auto result =
        is_from ? db_->get_connection_from(handle_name) : db_->get_connection_to(handle_name);
    struct ConnectionWrapper {
        std::string handle_from;
        std::string var_from;
        std::string handle_to;
        std::string var_to;

        [[nodiscard]] json11::Json to_json() const {
            return json11::Json::object{{{"handle_from", handle_from},
                                         {"var_from", var_from},
                                         {"handle_to", handle_to},
                                         {"var_to", var_to}}};
        }
    };
    std::vector<ConnectionWrapper> r;
    r.reserve(result.size());
    for (auto const &entry : result) {
        r.emplace_back(
            ConnectionWrapper{entry.handle_from, entry.var_from, entry.handle_to, entry.var_to});
    }
    auto content = json11::Json(r).dump();
    return content;
}

void initialize_runtime() {
    using namespace httplib;
    http_server = std::make_unique<Server>();

    // setup call backs
    http_server->Get("/breakpoint", [](const Request &req, Response &res) {
        auto bp = get_breakpoint(req.body, res);
    });

    http_server->Post("/breakpoint", [](const Request &req, Response &res) {
        auto bps = get_breakpoint(req.body, res);
        if (!bps.empty()) {
            for (auto const &bp : bps) {
                add_break_point(bp.first);
                add_breakpoint_expr(bp.first, bp.second);
                printf("Breakpoint inserted to %d\n", bp.first);
            }
        }
    });

    http_server->Delete("/breakpoint", [](const Request &req, Response &res) {
        auto bps = get_breakpoint(req.body, res);
        if (db_ && !bps.empty()) {
            for (auto const &bp : bps) {
                remove_break_point(bp.first);
                remove_expr(bp.first);
                printf("Breakpoint removed from %d\n", bp.first);
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
            remove_expr(id);
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
            remove_expr(bp);
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

    http_server->Delete("/monitor", [](const Request &req, Response &res) {
        printf("here\n");
        remove_all_monitor();
        res.status = 200;
        res.set_content("Okay", "text/plain");
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

    http_server->Post(R"(/clock/(\w+))", [](const Request &req, Response &res) {
        std::string value = req.matches[1];
        // set the bool to be true
        printf("pause on clock_edge %s\n", value.c_str());
        pause_clock_edge = value == "on";
        res.status = 200;
        res.set_content("Okay", "text/plain");
    });

    http_server->Post(R"(/hierarchy/([\w.$]+))", [](const Request &req, Response &res) {
        std::string name = req.matches[1];
        if (db_) {
            if (name == "$") name = "";
            auto result = db_->get_hierarchy(name);
            // set the current scope
            current_scope = name;

            std::vector<std::string> names;
            names.reserve(result.size());
            for (auto const &h : result) {
                names.emplace_back(fmt::format("{0}.{1}", h.parent_handle, h.child));
            }
            std::string content;
            if (has_paused_on_clock) {
                auto values = get_graph_value();
                content =
                    json11::Json(json11::Json::object({{"name", names}, {"value", values}})).dump();
            } else {
                content = json11::Json(json11::Json::object({{"name", names}})).dump();
            }
            res.status = 200;
            res.set_content(content, "application/json");
        } else {
            res.status = 403;
            res.set_content("[]", "application/json");
        }
    });

    http_server->Get(R"(/connection/to/([\w.$]+))", [](const Request &req, Response &res) {
        auto const handle_name = req.matches[1];
        if (db_) {
            auto content = get_connection_str(handle_name, false);
            res.status = 200;
            res.set_content(content, "application/json");
        } else {
            res.status = 403;
            res.set_content("[]", "application/json");
        }
    });

    http_server->Get(R"(/connection/from/([\w.$]+))", [](const Request &req, Response &res) {
        auto const handle_name = req.matches[1];
        if (db_) {
            auto content = get_connection_str(handle_name, true);
            res.status = 200;
            res.set_content(content, "application/json");
        } else {
            res.status = 403;
            res.set_content("[]", "application/json");
        }
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
                    printf("Debugger connected to %s:%d\n", ip.c_str(), port);
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

    // stop
    http_server->Post("/stop", [](const Request &req, Response &res) {
        printf("stop\n");
        // stop the simulation
        vpi_control(vpiFinish, 1);
        // unlock it if it's locked
        runtime_lock.unlock();
        res.status = 200;
        res.set_content("Okay", "text/plain");
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
