#include "BrokerRuntime.hpp"

#include "ipc/SocketConfig.hpp"
#include "messaging/MessageCodec.hpp"

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <algorithm>
#include <iostream>
#include <poll.h>
#include <sstream>
#include <sys/socket.h>
#include <unistd.h>
#include <utility>
#include <vector>

BrokerRuntime::BrokerRuntime(std::string socketPath)
    : server_{std::move(socketPath)}
{
}

int BrokerRuntime::run()
{
    if (!server_.start())
    {
        std::cerr << "Failed to start the socket server, terminating...\n";
        return 1;
    }

    std::cout << "[broker] is listening on " << ipc::BrokerSocketPath << "\n";

    const int listenFd = server_.listeningFd();
    if (listenFd < 0)
    {
        std::cerr << "[broker] listening fd is invalid\n";
        return 1;
    }

    if (!setNonBlocking(listenFd))
    {
        std::cerr << "[broker] failed to set non-blocking listen socket: "
                  << std::strerror(errno) << '\n';
        return 1;
    }

    while (true)
    {
        std::vector<pollfd> pollSet;
        pollSet.reserve(1 + clientBuffers_.size());
        pollSet.push_back({listenFd, POLLIN, 0});
        for (const auto& [fd, _] : clientBuffers_)
        {
            short events = POLLIN;
            const auto outIt = outboundFrames_.find(fd);
            if (outIt != outboundFrames_.end() && !outIt->second.empty())
            {
                events = static_cast<short>(events | POLLOUT);
            }
            pollSet.push_back({fd, events, 0});
        }

        const int readyCount = ::poll(pollSet.data(), pollSet.size(), -1);
        if (readyCount < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }

            std::cerr << "[broker] poll failed: " << std::strerror(errno) << '\n';
            return 1;
        }

        if ((pollSet[0].revents & POLLIN) != 0)
        {
            acceptPendingClients();
        }

        for (std::size_t i = 1; i < pollSet.size(); ++i)
        {
            const int clientFd = pollSet[i].fd;
            const short events = pollSet[i].revents;

            if (events == 0)
            {
                continue;
            }

            if ((events & POLLIN) != 0)
            {
                processClientRead(clientFd);
            }

            if ((events & POLLOUT) != 0)
            {
                processClientWrite(clientFd);
            }

            if ((events & (POLLHUP | POLLERR | POLLNVAL)) != 0)
            {
                closeClient(clientFd);
            }
        }
    }
}

bool BrokerRuntime::setNonBlocking(int fd) const
{
    const int currentFlags = ::fcntl(fd, F_GETFL, 0);
    if (currentFlags == -1)
    {
        return false;
    }

    return ::fcntl(fd, F_SETFL, currentFlags | O_NONBLOCK) != -1;
}

void BrokerRuntime::closeClient(int clientFd)
{
    if (clientBuffers_.find(clientFd) == clientBuffers_.end())
    {
        return;
    }

    const std::string disconnectedClient = clientLabel(clientFd);

    ::close(clientFd);
    clientBuffers_.erase(clientFd);
    clientIds_.erase(clientFd);
    outboundFrames_.erase(clientFd);

    const auto subscriptionsIt = clientSubscriptions_.find(clientFd);
    if (subscriptionsIt != clientSubscriptions_.end())
    {
        for (const auto& topic : subscriptionsIt->second)
        {
            const auto topicIt = topicSubscribers_.find(topic);
            if (topicIt != topicSubscribers_.end())
            {
                topicIt->second.erase(clientFd);
                if (topicIt->second.empty())
                {
                    topicSubscribers_.erase(topicIt);
                }
            }
        }
        clientSubscriptions_.erase(subscriptionsIt);
    }

    std::cout << "[broker] client disconnected, client=" << disconnectedClient << "\n";
    logBrokerState("after disconnect");
}

void BrokerRuntime::acceptPendingClients()
{
    while (true)
    {
        const int clientFd = server_.acceptClient();
        if (clientFd < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                break;
            }
            if (errno == EINTR)
            {
                continue;
            }

            std::cerr << "[broker] accept failed: " << std::strerror(errno) << '\n';
            break;
        }

        if (!setNonBlocking(clientFd))
        {
            std::cerr << "[broker] failed to set non-blocking client socket, fd="
                      << clientFd << '\n';
            ::close(clientFd);
            continue;
        }

        clientBuffers_.emplace(clientFd, std::string{});
        clientIds_.emplace(clientFd, "");
        outboundFrames_.emplace(clientFd, std::deque<std::string>{});
        clientSubscriptions_.emplace(clientFd, std::unordered_set<std::string>{});
        std::cout << "[broker] client connected, client=" << clientLabel(clientFd) << "\n";
        logBrokerState("after connect");
    }
}

void BrokerRuntime::processClientRead(int clientFd)
{
    bool disconnectClient = false;
    while (!disconnectClient)
    {
        char readBuffer[512];
        const ssize_t bytesRead = ::recv(clientFd, readBuffer, sizeof(readBuffer), 0);

        if (bytesRead > 0)
        {
            auto& pending = clientBuffers_[clientFd];
            pending.append(readBuffer, static_cast<std::size_t>(bytesRead));

            if (pending.size() > ipc::MaximumMessageSize)
            {
                std::cerr << "[broker] incoming message exceeds limit, fd=" << clientFd << '\n';
                disconnectClient = true;
                break;
            }

            std::size_t newlinePos = std::string::npos;
            while ((newlinePos = pending.find('\n')) != std::string::npos)
            {
                std::string incomingJson = pending.substr(0, newlinePos);
                pending.erase(0, newlinePos + 1);

                if (!incomingJson.empty() && incomingJson.back() == '\r')
                {
                    incomingJson.pop_back();
                }

                if (incomingJson.empty())
                {
                    continue;
                }

                std::string error;
                const auto message = messaging::MessageCodec::deserialize(incomingJson, error);
                if (!message)
                {
                    std::cerr << "[broker] invalid message: " << error << '\n';
                    continue;
                }

                handleIncomingMessage(clientFd, *message);
                dispatcher_.handleMessage(*message);
            }

            continue;
        }

        if (bytesRead == 0)
        {
            disconnectClient = true;
            break;
        }

        if (errno == EINTR)
        {
            continue;
        }

        if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
            break;
        }

        std::cerr << "[broker] recv failed, fd=" << clientFd
              << " error=" << std::strerror(errno) << '\n';
        disconnectClient = true;
    }

    if (disconnectClient)
    {
        closeClient(clientFd);
    }
}

void BrokerRuntime::processClientWrite(int clientFd)
{
    const auto queueIt = outboundFrames_.find(clientFd);
    if (queueIt == outboundFrames_.end())
    {
        return;
    }

    auto& queue = queueIt->second;
    while (!queue.empty())
    {
        std::string& frame = queue.front();
        const ssize_t bytesSent = ::send(clientFd, frame.data(), frame.size(), MSG_NOSIGNAL);

        if (bytesSent > 0)
        {
            const std::size_t sentCount = static_cast<std::size_t>(bytesSent);
            if (sentCount == frame.size())
            {
                queue.pop_front();
            }
            else
            {
                frame.erase(0, sentCount);
                break;
            }
            continue;
        }

        if (bytesSent == -1 && errno == EINTR)
        {
            continue;
        }

        if (bytesSent == -1 && (errno == EAGAIN || errno == EWOULDBLOCK))
        {
            break;
        }

        std::cerr << "[broker] send failed, fd=" << clientFd
                  << " error=" << std::strerror(errno) << '\n';
        closeClient(clientFd);
        return;
    }
}

void BrokerRuntime::handleIncomingMessage(int clientFd, const messaging::Message& message)
{
    if (!message.clientId.empty())
    {
        const auto clientIt = clientIds_.find(clientFd);
        if (clientIt != clientIds_.end() && clientIt->second != message.clientId)
        {
            clientIt->second = message.clientId;
            std::cout << "[broker] client identified, fd=" << clientFd
                      << " client_id=" << message.clientId << '\n';
        }
    }

    switch (message.type)
    {
        case messaging::MessageType::Subscribe:
            handleSubscribe(clientFd, message);
            break;

        case messaging::MessageType::Publish:
            handlePublish(clientFd, message);
            break;

        case messaging::MessageType::Ack:
            handleAck(clientFd, message);
            break;

        case messaging::MessageType::Deliver:
            std::cerr << "[broker] client cannot send deliver messages\n";
            break;

        case messaging::MessageType::Error:
            std::cerr << "[broker] error message received from client\n";
            break;
    }
}

void BrokerRuntime::handleSubscribe(int clientFd, const messaging::Message& message)
{
    if (message.topic.empty())
    {
        std::cerr << "[broker] subscribe ignored, empty topic fd=" << clientFd << '\n';
        return;
    }

    topicSubscribers_[message.topic].insert(clientFd);
    clientSubscriptions_[clientFd].insert(message.topic);

    std::cout << "[broker] subscription registered topic=" << message.topic
              << " fd=" << clientFd << '\n';
    logBrokerState("after subscribe");
}

void BrokerRuntime::handlePublish(int clientFd, const messaging::Message& message)
{
    const auto subscribersIt = topicSubscribers_.find(message.topic);
    if (subscribersIt == topicSubscribers_.end() || subscribersIt->second.empty())
    {
        return;
    }

    messaging::Message deliver = message;
    deliver.type = messaging::MessageType::Deliver;

    for (const int subscriberFd : subscribersIt->second)
    {
        if (subscriberFd == clientFd)
        {
            continue;
        }

        enqueueMessage(subscriberFd, deliver);
    }

    std::cout << "[broker] publish routed topic=" << message.topic
              << " subscriber_count=" << subscribersIt->second.size() << '\n';
    logBrokerState("after publish route");
}

void BrokerRuntime::handleAck(int /*clientFd*/, const messaging::Message& /*message*/)
{
    // ACK tracking/retry semantics are intentionally deferred to the next milestone.
}

void BrokerRuntime::enqueueMessage(int clientFd, const messaging::Message& message)
{
    const auto outIt = outboundFrames_.find(clientFd);
    if (outIt == outboundFrames_.end())
    {
        return;
    }

    auto& queue = outIt->second;
    if (queue.size() >= MaxQueueDepthPerClient)
    {
        if (message.qos == messaging::QoS::AtMostOnce)
        {
            queue.pop_front();
            std::cout << "[broker] queue full qos=0, dropped oldest frame client="
                      << clientLabel(clientFd)
                      << " topic=" << message.topic
                      << " max_depth=" << MaxQueueDepthPerClient << '\n';
        }
        else
        {
            std::cerr << "[broker] queue full qos=1, rejected new frame client="
                      << clientLabel(clientFd)
                      << " topic=" << message.topic
                      << " max_depth=" << MaxQueueDepthPerClient << '\n';
            return;
        }
    }

    std::string frame = messaging::MessageCodec::serialize(message);
    frame.push_back('\n');
    queue.push_back(std::move(frame));
}

std::string BrokerRuntime::formatConnectedClients() const
{
    std::vector<int> fds;
    fds.reserve(clientBuffers_.size());
    for (const auto& [fd, _] : clientBuffers_)
    {
        fds.push_back(fd);
    }
    std::sort(fds.begin(), fds.end());

    std::ostringstream output;
    output << "[";
    for (std::size_t i = 0; i < fds.size(); ++i)
    {
        if (i > 0)
        {
            output << ", ";
        }
        output << clientLabel(fds[i]);
    }
    output << "]";

    return output.str();
}

std::string BrokerRuntime::formatTopicSubscribers() const
{
    if (topicSubscribers_.empty())
    {
        return "{}";
    }

    std::vector<std::string> topics;
    topics.reserve(topicSubscribers_.size());
    for (const auto& [topic, _] : topicSubscribers_)
    {
        topics.push_back(topic);
    }
    std::sort(topics.begin(), topics.end());

    std::ostringstream output;
    output << "{";

    for (std::size_t topicIndex = 0; topicIndex < topics.size(); ++topicIndex)
    {
        if (topicIndex > 0)
        {
            output << ", ";
        }

        const auto& topic = topics[topicIndex];
        output << topic << ": [";

        const auto subscriberIt = topicSubscribers_.find(topic);
        if (subscriberIt != topicSubscribers_.end())
        {
            std::vector<int> subscribers(subscriberIt->second.begin(), subscriberIt->second.end());
            std::sort(subscribers.begin(), subscribers.end());

            for (std::size_t i = 0; i < subscribers.size(); ++i)
            {
                if (i > 0)
                {
                    output << ", ";
                }
                output << clientLabel(subscribers[i]);
            }
        }

        output << "]";
    }

    output << "}";
    return output.str();
}

std::string BrokerRuntime::clientLabel(int clientFd) const
{
    const auto clientIt = clientIds_.find(clientFd);
    const std::string clientId =
        (clientIt != clientIds_.end() && !clientIt->second.empty())
            ? clientIt->second
            : "pending-client-id";

    std::ostringstream output;
    output << clientId << "(fd=" << clientFd << ")";
    return output.str();
}

void BrokerRuntime::logBrokerState(const std::string& reason) const
{
    std::cout << "[broker] state " << reason
              << " connected_clients=" << formatConnectedClients()
              << " topic_subscribers=" << formatTopicSubscribers()
              << '\n';
}
