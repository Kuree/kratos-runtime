#include "control.hh"
#include <unistd.h>
#include <filesystem>
#include <iostream>
#include <memory>
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
SpinLock runtime_lock;
std::thread runtime_thread;
std::unordered_map<vpiHandle, std::string> monitored_signal_names;
std::unordered_map<std::string, vpiHandle> signal_call_back;
std::unique_ptr<Database> db_;
// include the dot to make things easier
std::string top_name_ = "TOP.";  // NOLINT

std::pair<bool, int64_t> get_value(const std::string &handle_name);

std::string get_breakpoint_value(uint32_t id) {
    std::vector<std::pair<std::string, std::string>> vars;
    std::map<std::string, std::pair<std::string, std::string>> variables;
    if (db_) {
        variables = db_->get_variable_mapping(id);
        for (auto const &[front_var, entry] : variables) {
            auto [handle_name, var] = entry;
            // decide if we need to append the top name
            if (handle_name.size() < top_name_.size() ||
                handle_name.substr(top_name_.size()) != top_name_) {
                std::string format;
                if (top_name_.back() == '.')
                    format = "{0}{1}.{2}";
                else
                    format = "{0}.{1}.{2}";
                handle_name = fmt::format(format, top_name_, handle_name, var);
            }
            vars.emplace_back(std::make_pair(front_var, handle_name));
        }
    }

    auto obj = json11::Json::object();
    json11::Json result = json11::Json::object({{"id", fmt::format("{0}", id)}, {"value", vars}});
    return result.dump();
}

void breakpoint_trace(uint32_t id) {
    if (!should_continue_simulation(id)) {
        // tell the client that we have hit a clock
        if (http_client) {
            auto content = get_breakpoint_value(id);
            http_client->Post("/status/breakpoint", content, "text/plain");
        }
        // hold the lock
        runtime_lock.lock();
    }
}

std::optional<uint32_t> get_breakpoint(const std::string &body, httplib::Response &res) {
    std::string error;
    auto json = json11::Json::parse(body, error);
    auto filename = json["filename"];
    auto line_num = json["line_num"];
    if (error.empty() && !filename.is_null() && !line_num.is_null()) {
        auto const &filename_str = filename.string_value();
        auto line_num_int = line_num.int_value();
        if (db_) {
            auto bp = db_->get_breakpoint_id(filename_str, line_num_int);
            if (bp) {
                res.status = 200;
                std::string s = get_breakpoint_value(bp.value());
                res.set_content(s, "text/plain");
                return bp;
            }
        }
    }
    if (error.empty()) {
        error = "ERROR";
    }
    res.status = 401;
    res.set_content(error, "text/plain");
    return std::nullopt;
}

std::pair<bool, int64_t> get_value(const std::string &handle_name) {
    auto handle = const_cast<char *>(handle_name.c_str());
    vpiHandle vh = vpi_handle_by_name(handle, nullptr);
    if (!vh) {
        // not found
        return {false, 0};
    } else {
        s_vpi_value v;
        v.format = vpiIntVal;
        vpi_get_value(vh, &v);
        int64_t result = v.value.integer;
        return {true, result};
    }
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
    http_server->Post("/breakpoint", [](const Request &req, Response &res) {
        auto bp = get_breakpoint(req.body, res);
        if (db_ && bp) {
            if (bp) {
                add_break_point(*bp);
                printf("Breakpoint inserted to %d\n", *bp);
                return;
            }
        } else {
            res.status = 401;
            res.set_content("ERROR", "text/plain");
        }
    });

    http_server->Delete("/breakpoint", [](const Request &req, Response &res) {
        auto bp = get_breakpoint(req.body, res);
        if (db_ && bp) {
            if (bp) {
                remove_break_point(*bp);
                printf("Breakpoint removed from %d\n", *bp);
                return;
            }
        } else {
            res.status = 401;
            res.set_content("ERROR", "text/plain");
        }
    });

    http_server->Get(R"(/value/([\w.$]+))", [](const Request &req, Response &res) {
        auto name = req.matches[1];
        auto result = get_value(name);
        if (result.first) {
            res.status = 200;
            auto content = fmt::format("{0}", result.second);
            res.set_content(content, "text/plain");
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
            auto port = port_json.number_value();
            auto &db_filename = db_json.string_value();

            if (!std::filesystem::exists(db_filename)) {
                has_error = true;
            } else {
                try {
                    http_client = std::make_unique<Client>(ip.c_str(), port);
                    // load up the database
                    db_ = std::make_unique<Database>(db_filename);
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
            printf("Debugger connected\n");
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

// these are system level calls. register it to the simulator
void (*vlog_startup_routines[])() = {initialize_runtime_vpi, nullptr};
