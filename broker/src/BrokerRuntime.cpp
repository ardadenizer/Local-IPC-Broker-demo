#include "BrokerRuntime.hpp"

#include "ipc/SocketConfig.hpp"
#include "messaging/MessageCodec.hpp"

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <poll.h>
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
            pollSet.push_back({fd, POLLIN, 0});
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

            // Read available data first. poll can report POLLIN and POLLHUP together
            // when the peer sent data and then closed; closing first would drop that data.
            if ((events & POLLIN) != 0)
            {
                processClientRead(clientFd);
                continue;
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
    ::close(clientFd);
    clientBuffers_.erase(clientFd);
    std::cout << "[broker] client disconnected, fd=" << clientFd << "\n";
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
        std::cout << "[broker] client connected, fd=" << clientFd << "\n";
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
