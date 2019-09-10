#include "control.hh"
#include <iostream>
#include <unordered_set>
#include "httplib.h"
#include "std/vpi_user.h"
#include "sim.hh"
#include "util.hh"
#include <unistd.h>

// constants
constexpr uint16_t runtime_port = 8888;

std::unique_ptr<httplib::Server> http_server = nullptr;
std::unique_ptr<httplib::Client> http_client = nullptr;
SpinLock runtime_lock;
std::thread runtime_thread;

void breakpoint_trace(uint32_t id) {
    if (should_continue_simulation(id)) {
        // tell the client that we have hit a clock
        if (http_client) {
            http_client->Post("/status/breakpoint", "", "text/plain");
        }
        // hold the lock
        runtime_lock.lock();
    }
}

std::pair<bool, uint32_t> get_breakpoint(const std::string &num, httplib::Response &res) {
    bool error = false;
    uint32_t break_point = 0;
    try {
        break_point = static_cast<uint32_t>(std::stoi(num));
    } catch (const std::invalid_argument &) {
        error = true;
    } catch (const std::out_of_range &) {
        error = false;
    }
    if (error) {
        res.status = 401;
        res.set_content("Error", "text/plain");
    } else {
        res.status = 200;
        res.set_content("Okay", "text/plain");
    }
    return {!error, break_point};
}

void initialize_runtime() {
    using namespace httplib;
    http_server = std::unique_ptr<Server>(new Server());

    // setup call backs
    http_server->Post(R"(/breakpoint/add/(\d+))", [](const Request &req, Response &res) {
        auto num = req.matches[1];
        auto result = get_breakpoint(num, res);

        if (result.first) add_break_point(result.second);
    });

    http_server->Post(R"(/breakpoint/remove/(\d+))", [](const Request &req, Response &res) {
        auto num = req.matches[1];
        auto result = get_breakpoint(num, res);

        if (result.first)
            remove_break_point(result.second);
    });

    http_server->Post("/continue", [](const Request &req, Response &res) {
        runtime_lock.unlock();
        res.status = 200;
        res.set_content("Okay", "text/plain");
    });

    http_server->Post("/connect/", [](const Request &req, Response &res) {
        // parse the content
        auto body = req.body;
        auto tokens = get_tokens(body, ":");
        std::string ip, port_str;
        uint32_t port;
        // linux kernel style error handling
        if (tokens.size() != 2) {
            goto error;
        }
        ip = tokens[0];
        port_str = tokens[1];
        try {
            port = std::stoi(port_str);
            http_client = std::unique_ptr<Client>(new httplib::Client(ip.c_str(), port));
        } catch (...) {
            http_client = nullptr;
            goto error;
        }
        // unblock it
        runtime_lock.unlock();
    error:
        res.status = 401;
        res.set_content("ERROR", "text/plain");
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
