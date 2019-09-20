#include "server.hh"
#include <unordered_set>


// handler class
class ServerHandler: seasocks::WebSocket::Handler {
    std::unordered_set<seasocks::WebSocket*> conns_;

    void onConnect(seasocks::WebSocket* conn) override {
        conns_.emplace(conn);
    }

    void onDisconnect(seasocks::WebSocket* conn) override  {
        conns_.erase(conn);
    }

    void onData(seasocks::WebSocket *conn, const char *data) override {
    }
};

Server::Server(std::string ip, uint32_t port) : ip_(std::move(ip)), port_(port) {
    using namespace seasocks;
    logger_ = std::make_shared<PrintfLogger>(Logger::Level::Error);
    ws_server_ = std::unique_ptr<seasocks::Server>(new seasocks::Server(logger_));
}
