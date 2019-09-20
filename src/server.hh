#ifndef KRATOS_RUNTIME_SERVER_HH
#define KRATOS_RUNTIME_SERVER_HH

#include "seasocks/Server.h"
#include "seasocks/PrintfLogger.h"

class Server {
public:
    Server(std::string ip, uint32_t port);
    void run();
    void on(const std::string &path, const std::function<void(const std::string &)>&call_back);

private:
    std::unique_ptr<seasocks::Server> ws_server_ = nullptr;
    std::shared_ptr<seasocks::PrintfLogger> logger_ = nullptr;

    std::string ip_;
    uint32_t port_;
};

#endif  // KRATOS_RUNTIME_SERVER_HH
