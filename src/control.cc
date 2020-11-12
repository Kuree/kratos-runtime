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
#include "rapidjson/document.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"
#include "sim.hh"
#include "std/vpi_user.h"
#include "util.hh"

// constants
uint16_t runtime_port = 8888;
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
// this is used for remote debugging
// src_path is where the user's folder is
// dst_path is where the code is compiled on the server
std::string src_path;
std::string dst_path;
// mutex
std::mutex runtime_lock;
std::mutex vpi_lock;
// step over. notice that this is not mutex protected. You should not set step over during
// the simulation
bool step_over = false;
// is the simulation paused
bool paused = false;
// whether to pause at the the clock edge
bool pause_clock_edge = false;
// current scope it has to be set to get the connections
std::string current_scope;
// we don't want to send back the values if it's never stopped
bool has_paused_on_clock = false;
// if no client server, we don't need to send information back
bool use_client_request = false;

std::optional<std::string> get_value(std::string handle_name);
std::optional<std::string> get_simulation_time(const std::string &);
bool evaluate_breakpoint_expr(uint32_t breakpoint_id);

// convert the [] name to . for arrays
std::string process_var_front_name(const std::string &name);

struct CbHandle {
    s_vpi_time time;
    s_vpi_value value;
    s_cb_data cb_data;
    vpiHandle cb_handle = nullptr;
    char *name = nullptr;
};

void pause_sim() {
    paused = true;
    runtime_lock.lock();
}

void un_pause_sim() {
    paused = false;
    runtime_lock.unlock();
}

// this is for vpi cb struct
std::unordered_map<std::string, CbHandle *> cb_handle_map;

template <class T>
std::string to_string(T &d) {
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    d.Accept(writer);

    std::string result = buffer.GetString();
    // return by value
    return result;
}

const rapidjson::Value &get_json_value(const rapidjson::Value &v, rapidjson::Document &) {
    // noop
    return v;
}

rapidjson::Value get_json_value(const std::string &v, rapidjson::Document &d) {
    rapidjson::Value value(rapidjson::kStringType);
    value.SetString(v.c_str(), d.GetAllocator());
    return value;
}

rapidjson::Value get_json_value(uint32_t v, rapidjson::Document &d) {
    rapidjson::Value value(rapidjson::kNumberType);
    value.SetInt(static_cast<int>(v));
    return value;
}

rapidjson::Value get_json_value(const std::vector<std::pair<std::string, std::string>> &array,
                                rapidjson::Document &d) {
    rapidjson::Value v(rapidjson::kArrayType);
    for (auto const &[name, value] : array) {
        rapidjson::Value entry(rapidjson::kObjectType);
        entry.AddMember(get_json_value(name, d), get_json_value(value, d), d.GetAllocator());
        v.PushBack(entry.Move(), d.GetAllocator());
    }
    return v;
}

rapidjson::Value get_json_value(const std::map<std::string, std::string> &map,
                                rapidjson::Document &d) {
    rapidjson::Value v(rapidjson::kArrayType);
    for (auto const &[name, value] : map) {
        rapidjson::Value entry(rapidjson::kObjectType);
        entry.AddMember(get_json_value(name, d), get_json_value(value, d), d.GetAllocator());
        v.PushBack(entry.Move(), d.GetAllocator());
    }
    return v;
}

rapidjson::Value get_json_value(const std::vector<std::string> &array, rapidjson::Document &d) {
    rapidjson::Value v(rapidjson::kArrayType);
    for (auto const &value : array) {
        auto entry = get_json_value(value, d);
        v.PushBack(entry.Move(), d.GetAllocator());
    }
    return v;
}

template <class T>
void add_json_member(const std::string &name, const T &value, rapidjson::Document &d) {
    if constexpr (std::is_base_of<const rapidjson::Value&, T>()) {
        d.AddMember(get_json_value(name, d), value, d.GetAllocator());
    } else {
        d.AddMember(get_json_value(name, d), get_json_value(value, d), d.GetAllocator());
    }
}

template <class T>
void add_json_member(const std::string &name, const T &value, rapidjson::Value &v,
                     rapidjson::Document &d) {
    v.AddMember(get_json_value(name, d), get_json_value(value, d), d.GetAllocator());
}

std::string get_breakpoint_value(uint32_t instance_id, uint32_t id) {
    std::vector<std::pair<std::string, std::string>> gen_vars;
    std::vector<std::pair<std::string, std::string>> local_vars;
    if (db_) {
        auto variables = db_->get_variable_mapping(instance_id, id);
        for (auto const &variable : variables) {
            // decide if we need to append the top name
            if (variable.is_var) {
                auto value = get_value(fmt::format("{0}.{1}", variable.handle, variable.value));
                std::string v;
                if (value)
                    v = value.value();
                else
                    v = "ERROR";
                // we need some process here to replace the [] in array notion, if any
                auto var_name = process_var_front_name(variable.value);
                gen_vars.emplace_back(std::make_pair(var_name, v));
            } else {
                gen_vars.emplace_back(variable.name, variable.value);
            }
        }
        auto context_vars = db_->get_context_variable(instance_id, id);
        for (auto const &variable : context_vars) {
            if (variable.is_var) {
                auto handle_name = fmt::format("{0}.{1}", variable.handle, variable.value);
                auto value = get_value(handle_name);
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
    std::string instance_name;
    if (db_) {
        auto bp = db_->get_breakpoint_info(id);
        if (bp) {
            filename = bp.value().first;
            line_num = fmt::format("{0}", bp.value().second);
            // convert them if necessary
            // replace the path if necessary
            if (!src_path.empty() && !dst_path.empty()) {
                replace(filename, dst_path, src_path);
            }
        }
        instance_name = db_->get_instance_name(instance_id);
    }
    rapidjson::Document d(rapidjson::kObjectType);

    add_json_member("id", id, d);
    add_json_member("local", local_vars, d);
    add_json_member("generator", gen_vars, d);
    add_json_member("filename", filename, d);
    add_json_member("line_num", line_num, d);
    add_json_member("instance_name", instance_name, d);
    add_json_member("instance_id", instance_id, d);

    std::string result = to_string(d);
    // return by value
    return result;
}

std::string process_var_front_name(const std::string &name) {
    std::string var_name;
    var_name.reserve(name.size());
    for (auto const c : name) {
        if (c == '[')
            var_name += '.';
        else if (c != ']')
            var_name += c;
    }
    return var_name;
}

void breakpoint_trace(uint32_t instance_id, uint32_t id) {
    if (step_over || !should_continue_simulation(id)) {
        printf("hit breakpoint %d step_over: %d\n", id, step_over);
        // if we have a conditional breakpoint
        // we need to check that
        if (has_expr_breakpoint(id)) {
            if (!evaluate_breakpoint_expr(id)) return;
        }
        // tell the client that we have hit a clock
        if (http_client) {
            auto content = get_breakpoint_value(instance_id, id);
            if (step_over) {
                http_client->Post("/status/step", content, "application/json");
            } else {
                http_client->Post("/status/breakpoint", content, "application/json");
            }
        }
        // pause the simulation
        // only pause when we know we can continue
        if (http_client || use_client_request) pause_sim();
    }
}

std::pair<std::string, std::map<std::string, std::string>> get_graph_value() {
    auto time_val = get_simulation_time("");
    std::string time = "ERROR";
    if (time_val) time = *time_val;
    if (!db_) return {};
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
    return std::make_pair(time, values);
}

std::string get_context_value(std::string filename, uint32_t line_num) {
    // get all the instance ids and line number
    if (!db_) {
        while (!db_) {
            // sleep for 0.1 seconds
            usleep(100000);
        }
    }
    if (db_) {
        // replace the path if necessary
        if (!src_path.empty() && !dst_path.empty()) {
            replace(filename, src_path, dst_path);
        }
        auto bps = db_->get_breakpoints(filename, line_num);
        if (!bps.empty()) {
            std::vector<std::string> result;
            result.reserve(bps.size());

            for (auto const &bp : bps) {
                auto v = get_breakpoint_value(bp.instance_id, bp.breakpoint_id);
                result.emplace_back(v);
            }

            rapidjson::Document d(rapidjson::kArrayType);
            auto &allocator = d.GetAllocator();
            for (auto &entry : result) {
                d.PushBack(get_json_value(entry, d).Move(), allocator);
            }
            return to_string(d);
        }
    }
    return "{}";
}

void breakpoint_clock(void) {
    if (pause_clock_edge) {
        has_paused_on_clock = true;
        printf("Pause on clock edge\n");
        if (http_client && db_) {
            rapidjson::Document d(rapidjson::kObjectType);
            auto const &[time, values] = get_graph_value();
            add_json_member("time", time, d);
            add_json_member("value", values, d);
            auto content = to_string(d);
            http_client->Post("/status/clock",
                              content,  // NOLINT
                              "application/json");
        }
        if (http_client || use_client_request) pause_sim();
    }
}

PLI_INT32 cb_pause_at_synch(s_cb_data *) {
    printf("paused on synch\n");
    if (http_client) {
        http_client->Post("/status/synch", "Okay", "plain/text");
    }
    pause_sim();
    return 0;
}

void pause_at_synch() {
    s_cb_data cb_data_init;
    cb_data_init.obj = nullptr;
    cb_data_init.index = 0;
    cb_data_init.value = nullptr;
    cb_data_init.reason = cbNextSimTime;
    cb_data_init.cb_rtn = &cb_pause_at_synch;
    cb_data_init.time = nullptr;

    vpiHandle res = vpi_register_cb(&cb_data_init);
    if (!res) {
        std::cerr << "ERROR: failed to register runtime initialization" << std::endl;
    }
}

void exception(uint32_t instance_id, uint32_t id) {
    if (http_client) {
        auto content = get_breakpoint_value(instance_id, id);
        http_client->Post("/status/exception", content, "application/json");
        pause_sim();
    }
}

std::string get_breakpoint_content(const std::vector<std::pair<uint32_t, uint32_t>> &bps) {
    rapidjson::Document d(rapidjson::kArrayType);
    auto &allocator = d.GetAllocator();
    for (auto const &[bp, col] : bps) {
        rapidjson::Value v(rapidjson::kObjectType);
        add_json_member("id", bp, v, d);
        add_json_member("col", col, v, d);
        d.PushBack(v.Move(), allocator);
    }
    auto content = to_string(d);
    return content;
}

void set_breakpoint_content(const std::vector<std::pair<uint32_t, uint32_t>> &bps,
                            httplib::Response &res) {
    res.status = 200;
    auto const content = get_breakpoint_content(bps);
    res.set_content(content, "application/json");
}

std::vector<std::pair<uint32_t, uint32_t>> get_breakpoint(const std::string &filename,
                                                          uint32_t line_num) {
    // hacky way
    if (!db_) {
        while (!db_) {
            // sleep for 0.1 seconds
            usleep(100000);
        }
    }
    if (db_) {
        auto bps = db_->get_breakpoint_id(filename, line_num);
        if (!bps.empty()) {
            std::vector<std::pair<uint32_t, uint32_t>> result;
            result.reserve(bps.size());
            for (auto const &bp : bps) {
                auto col = db_->get_breakpoint_column(bp);
                result.emplace_back(std::make_pair(bp, col));
            }
            return result;
        } else {
            std::cerr << "Unable to find breakpoint at " << filename << ":" << line_num
                      << std::endl;
        }
    }
    return {};
}

bool add_breakpoint_expr(uint32_t breakpoint_id, const std::string &expr) {
    if (expr.empty()) return true;
    if (!db_) return false;
    // query the local port variables
    auto op_id = db_->get_instance_id(breakpoint_id);
    if (!op_id) return false;
    auto const self_variables = db_->get_variable_mapping(*op_id, breakpoint_id);
    auto const context_variables = db_->get_context_variable(*op_id, breakpoint_id);
    std::unordered_map<std::string, int64_t> constants;
    std::unordered_set<std::string> symbols;
    // need to extract the time
    const static std::string time_var_name = "time";
    bool has_time_var_name_as_context = false;

    // initialize the mapping
    breakpoint_symbol_mapping[breakpoint_id] = {};
    // this is self variables
    for (auto const &v : self_variables) {
        auto front_var = v.name;
        // if front var is empty, it means it's generator variables
        if (front_var.empty()) continue;
        if (is_expr_symbol(expr, front_var)) {
            if (v.is_var) {
                // compute handle name
                auto handle_name = fmt::format("{0}.{1}", v.handle, v.value);
                handle_name = get_handle_name(top_name_, handle_name);
                breakpoint_symbol_mapping[breakpoint_id].emplace(front_var, handle_name);
                symbols.emplace(front_var);
            } else {
                try {
                    auto value = std::stoi(v.value);
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
        printf("Adding expr (%s) to breakpoint %d\n", expr.c_str(), breakpoint_id);
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

std::vector<uint32_t> get_breakpoint_filename(std::string filename, httplib::Response &res) {
    if (!filename.empty()) {
        if (db_) {
            res.status = 200;
            // return a list of breakpoints
            res.set_content("Okay", "text/plain");
            if (!src_path.empty() && !dst_path.empty()) replace(filename, src_path, dst_path);
            auto bps = db_->get_all_breakpoints(filename);

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
        printf("%s\n", handle_name.c_str());
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
        rapidjson::Document d;
        add_json_member("handle", signal_name, d);
        add_json_member("value", value, d);
        auto content = to_string(d);
        http_client->Post("/value", content, "application/json");
    }
    return 0;
}

bool setup_monitor(std::string signal_name) {
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

        cb_handle->cb_handle = r;

        printf("monitor added to %s\n", signal_name.c_str());

        return true;
    }
}

bool remove_monitor(std::string signal_name) {
    signal_name = get_handle_name(top_name_, signal_name);
    if (cb_handle_map.find(signal_name) == cb_handle_map.end()) return false;
    auto cb = cb_handle_map.at(signal_name);
    printf("monitor removed from %s\n", signal_name.c_str());

    auto r = vpi_remove_cb(cb->cb_handle);

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
    rapidjson::Document d(rapidjson::kArrayType);
    auto &allocator = d.GetAllocator();

    for (auto const &entry : result) {
        rapidjson::Value v(rapidjson::kObjectType);
        add_json_member("handle_from", entry.handle_from, v, d);
        add_json_member("var_from", entry.var_from, v, d);
        add_json_member("handle_to", entry.handle_to, v, d);
        add_json_member("var_to", entry.var_to, v, d);
        d.PushBack(v.Move(), allocator);
    }
    auto content = to_string(d);
    return content;
}

std::optional<std::pair<std::string, uint32_t>> get_fn_ln(const std::string &path) {
    std::string filename;
    uint32_t line_num = 0xFFFFFFFF;
    auto tokens = get_tokens(path, ":");
    if (tokens.size() == 2) {
        auto line_num_str = tokens.back();
        if (is_digits(line_num_str)) {
            line_num = std::stoul(line_num_str);
            filename = tokens[0];
        }
    } else if (tokens.size() > 2) {
        auto col_str = tokens[tokens.size() - 1];
        auto line_num_str = tokens[tokens.size() - 2];
        if (is_digits(col_str) && is_digits(line_num_str)) {
            // TODO:
            // add column str
            filename = join(tokens.begin(), tokens.begin() + (tokens.size() - 2), ":");
            line_num = std::stoul(line_num_str);
        }
    }

    if (filename.empty() || line_num == 0xFFFFFFFF)
        return std::nullopt;
    else
        return std::make_pair(filename, line_num);
}

std::optional<std::pair<uint32_t, std::string>> parse_bp_json(const std::string &content) {
    rapidjson::Document d;
    d.Parse(content.c_str());
    if (!d.HasParseError()) {
        if (d.HasMember("id") && d["id"].IsNumber()) {
            auto id = static_cast<uint32_t>(d["id"].GetInt());
            std::string expr;
            if (d.HasMember("expr") && d["expr"].IsString()) {
                expr = d["expr"].GetString();
            }
            return std::make_pair(id, expr);
        }
    }
    return std::nullopt;
}

void set_error(int error_code, const std::string &error_message, httplib::Response &res) {
    res.status = error_code;
    res.set_content(error_message, "text/plain");
}

void initialize_runtime() {
    using namespace httplib;
    http_server = std::make_unique<Server>();

    // setup call backs
    http_server->Get(R"(/breakpoint/(.*))", [](const Request &req, Response &res) {
        vpi_lock.lock();
        auto op_fn_ln = get_fn_ln(req.matches.size() > 1 ? req.matches[1].str() : "");
        if (op_fn_ln) {
            auto const &[fn, ln] = *op_fn_ln;
            auto bps = get_breakpoint(fn, ln);
            set_breakpoint_content(bps, res);
        } else {
            set_error(401, "Invalid breakpoint request", res);
        }
        vpi_lock.unlock();
    });

    http_server->Post("/breakpoint", [](const Request &req, Response &res) {
        vpi_lock.lock();
        auto bp_info = parse_bp_json(req.body);
        if (bp_info) {
            auto const &[bp_id, expr] = *bp_info;
            add_break_point(bp_id);
            if (!expr.empty()) {
                add_breakpoint_expr(bp_id, expr);
            }
        } else {
            set_error(401, "Invalid breakpoint request", res);
        }
        vpi_lock.unlock();
    });

    http_server->Delete("/breakpoint", [](const Request &req, Response &res) {
        vpi_lock.lock();
        auto op_fn_ln = get_fn_ln(req.matches.size() > 1 ? req.matches[1].str() : "");
        if (op_fn_ln) {
            auto const &[fn, ln] = *op_fn_ln;
            auto bps = get_breakpoint(fn, ln);
            if (db_ && !bps.empty()) {
                for (auto const &[id, col] : bps) {
                    remove_break_point(id);
                    remove_expr(id);

                    printf("Breakpoint removed from %d\n", id);
                }
            } else {
                res.status = 401;
                res.set_content("ERROR", "text/plain");
            }
        } else {
            set_error(401, "ERROR", res);
        }
        vpi_lock.unlock();
    });

    http_server->Delete(R"(/breakpoint/(\d+))", [](const Request &req, Response &res) {
        auto num = req.matches[1];
        vpi_lock.lock();
        try {
            auto id = std::stoi(num);
            remove_break_point(id);
            remove_expr(id);
            vpi_lock.unlock();
            printf("Breakpoint removed from %d\n", id);
            res.status = 200;
            res.set_content("Okay", "text/plain");
            return;
        } catch (...) {
            vpi_lock.unlock();
            res.status = 401;
            res.set_content("ERROR", "text/plain");
            return;
        }
    });

    // delete all breakpoint from a file
    http_server->Delete(R"(/breakpoint/file/(.*))", [](const Request &req, Response &res) {
        auto filename = req.matches[1];
        vpi_lock.lock();
        auto bps = get_breakpoint_filename(filename, res);
        for (auto const &bp : bps) {
            remove_break_point(bp);
            remove_expr(bp);
        }
        vpi_lock.unlock();
    });

    // get all the files
    http_server->Get("/files", [](const Request &req, Response &res) {
        if (db_) {
            auto names = db_->get_all_files();
            rapidjson::Document d(rapidjson::kArrayType);
            auto &allocator = d.GetAllocator();
            for (auto const &name : names) {
                d.PushBack(get_json_value(name, d), allocator);
            }
            auto content = to_string(d);
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
        rapidjson::Document d;
        d.Parse(req.body.c_str());
        if (!d.HasParseError()) {
            auto const &lst = d.GetArray();
            rapidjson::Document result(rapidjson::kArrayType);
            auto &allocator = result.GetAllocator();
            for (auto const &entry : lst) {
                std::string name = entry.GetString();
                auto v = get_value(name);
                std::string value;
                if (v) {
                    value = v.value();

                } else {
                    value = "ERROR";
                }
                rapidjson::Value e_v(rapidjson::kObjectType);
                add_json_member("name", name, e_v, result);
                add_json_member("value", value, e_v, result);
                result.PushBack(e_v.Move(), allocator);
            }
            res.status = 200;
            auto content = to_string(result);
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
        vpi_lock.lock();
        auto result = setup_monitor(name);
        vpi_lock.unlock();
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
        vpi_lock.lock();
        auto result = remove_monitor(name);
        vpi_lock.unlock();
        if (result) {
            res.status = 200;
            res.set_content("Okay", "text/plain");
        } else {
            res.status = 401;
            res.set_content("ERROR", "text/plain");
        }
    });

    http_server->Delete("/monitor", [](const Request &req, Response &res) {
        vpi_lock.lock();
        remove_all_monitor();
        vpi_lock.unlock();
        res.status = 200;
        res.set_content("Okay", "text/plain");
    });

    http_server->Post("/continue", [](const Request &req, Response &res) {
        step_over = false;
        un_pause_sim();
        res.status = 200;
        res.set_content("Okay", "text/plain");
    });

    http_server->Post("/step_over", [](const Request &req, Response &res) {
        step_over = true;
        un_pause_sim();
        res.status = 200;
        res.set_content("Okay", "text/plain");
    });

    http_server->Post("/top_name", [](const Request &req, Response &res) {
        std::string value = req.body;
        top_name_ = value + ".";
        res.status = 200;
        res.set_content("Okay", "text/plain");
    });

    http_server->Post(R"(/clock/(\w+))", [](const Request &req, Response &res) {
        std::string value = req.matches[1];
        // set the bool to be true
        printf("pause on clock_edge %s\n", value.c_str());
        if (value == "synch") pause_at_synch();
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
                rapidjson::Document d(rapidjson::kObjectType);
                auto const &[time, values] = get_graph_value();
                add_json_member("name", names, d);
                rapidjson::Value v;
                add_json_member("value", v, d);
                content = to_string(d);
            } else {
                rapidjson::Document d(rapidjson::kObjectType);
                add_json_member("name", names, d);
                content = to_string(d);
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
        std::string ip, db_filename;
        int port;
        rapidjson::Document d;
        d.Parse(req.body.c_str());
        bool has_error = d.HasParseError();
        if (!d.HasMember("ip") || !d.HasMember("port") || !d.HasMember("database")) {
            has_error = true;
        } else {
            auto &ip_json = d["ip"];
            auto &port_json = d["port"];
            auto &db_json = d["database"];

            // this is a short cut
            if (ip_json.IsString()) {
                if (std::string(ip_json.GetString()) == "255.255.255.255") {
                    // this is the client no server mode
                    res.status = 200;
                    res.set_content("Okay", "text/plain");
                    use_client_request = true;
                    return;
                }
            }

            // this is for remote debugging path translation
            if (d.HasMember("src_path") && d.HasMember("dst_path")) {
                auto &src_path_json = d["src_path"];
                auto &dst_path_json = d["dst_path"];
                if (src_path_json.IsString() && dst_path_json.IsString()) {
                    src_path = src_path_json.GetString();
                    dst_path = dst_path_json.GetString();
                } else {
                    has_error = true;
                }
            }

            if (!port_json.IsNumber() || !ip_json.IsString() || !db_json.IsString()) {
                has_error = true;
            }
            if (!has_error) {
                ip = ip_json.GetString();
                port = static_cast<int>(port_json.GetInt());
                db_filename = db_json.GetString();
                // convert db_filename to the dst one
                if (!src_path.empty() && !dst_path.empty()) {
                    replace(db_filename, src_path, dst_path);
                }

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
        un_pause_sim();
        res.status = 200;
        res.set_content("Okay", "text/plain");
    });

    // get status
    http_server->Get("/status", [](const Request &req, Response &res) {
        std::string result;
        if (db_)
            result = "Connected";
        else
            result = "Disconnected";
        res.status = 200;
        res.set_content(result, "text/plain");
    });

    // get simulation status
    http_server->Get("/status/simulation", [](const Request &req, Response &res) {
        std::string result;
        if (paused)
            result = "Paused";
        else
            result = "Running";
        res.status = 200;
        res.set_content(result, "text/plain");
    });

    // get context info based on filename and line number
    http_server->Get("/context/(.*)", [](const Request &req, Response &res) {
        auto const &body = req.body;
        auto const fn_ln = req.matches[1];
        auto tokens = get_tokens(fn_ln, ":");
        std::string result;
        if (tokens.size() == 2) {
            auto const &filename = tokens[0];
            auto line_num_value = static_cast<uint32_t>(std::stoi(tokens[1]));
            result = get_context_value(filename, line_num_value);
        } else {
            res.status = 401;
            result = "[]";
        }
        res.set_content(result, "application/json");
    });

    // start the http in a different thread
    // get port number from environment variable
    auto env_port_s = std::getenv("KRATOS_PORT");
    if (env_port_s) {
        try {
            auto value = std::stoi(env_port_s);
            runtime_port = static_cast<uint16_t>(value);
        } catch (const std::invalid_argument &) {
            std::cerr << "Unable to set port to " << env_port_s;
        }
    }
    runtime_thread = std::thread([=]() {
        std::cout << "Kratos runtime server runs at 0.0.0.0:" << runtime_port << std::endl;
        auto r = http_server->listen("0.0.0.0", runtime_port);
        if (!r) {
            std::cerr << "Unable to start server at 0.0.0.0:" << runtime_port << std::endl;
            return;
        }
    });

    // by default it's locked
    runtime_lock.lock();
    pause_sim();
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
    un_pause_sim();
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
