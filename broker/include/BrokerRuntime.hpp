#pragma once

#include "UnixSocketServer.hpp"
#include "MessageDispatcher.hpp"

#include <string>
#include <unordered_map>

class BrokerRuntime
{
public:
    explicit BrokerRuntime(std::string socketPath);
    int run();

private:
    bool setNonBlocking(int fd) const;
    void closeClient(int clientFd);
    void acceptPendingClients();
    void processClientRead(int clientFd);

    UnixSocketServer server_;
    MessageDispatcher dispatcher_;
    std::unordered_map<int, std::string> clientBuffers_;
};
