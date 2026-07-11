#pragma once

#include "UnixSocketServer.hpp"
#include "MessageDispatcher.hpp"
#include "messaging/Message.hpp"

#include <deque>
#include <string>
#include <unordered_set>
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
    void processClientWrite(int clientFd);
    void handleIncomingMessage(int clientFd, const messaging::Message& message);
    void handleSubscribe(int clientFd, const messaging::Message& message);
    void handlePublish(int clientFd, const messaging::Message& message);
    void handleAck(int clientFd, const messaging::Message& message);
    void enqueueMessage(int clientFd, const messaging::Message& message);
    std::string clientLabel(int clientFd) const;
    std::string formatConnectedClients() const;
    std::string formatTopicSubscribers() const;
    void logBrokerState(const std::string& reason) const;

    UnixSocketServer server_;
    MessageDispatcher dispatcher_;
    std::unordered_map<int, std::string> clientBuffers_;
    std::unordered_map<int, std::string> clientIds_;
    std::unordered_map<int, std::deque<std::string>> outboundFrames_;
    std::unordered_map<int, std::unordered_set<std::string>> clientSubscriptions_;
    std::unordered_map<std::string, std::unordered_set<int>> topicSubscribers_;
};
