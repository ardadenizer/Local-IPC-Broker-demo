#include "UnixSocketServer.hpp"

#include "ipc/SocketConfig.hpp"

#include <cerrno>
#include <cstring>
#include <iostream>

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

UnixSocketServer::UnixSocketServer(std::string socketPath)
    : socketPath_{std::move(socketPath)}
{
}

UnixSocketServer::~UnixSocketServer()
{
    stop();
}

bool UnixSocketServer::start()
{
    if (socketPath_.size() >= sizeof(sockaddr_un::sun_path))
    {
        std::cerr << "[broker] socket path is too long\n";
        return false;
    }

    serverFd_ = ::socket(AF_UNIX, SOCK_STREAM, 0);

    if (serverFd_ == -1)
    {
        std::cerr << "[broker] socket creation failed: "
                  << std::strerror(errno) << '\n';

        return false;
    }

    // Remove a stale filesystem entry left by an earlier broker.
    ::unlink(socketPath_.c_str());

    sockaddr_un address{};
    address.sun_family = AF_UNIX;

    std::memcpy(
        address.sun_path,
        socketPath_.data(),
        socketPath_.size());

    address.sun_path[socketPath_.size()] = '\0';

    if (::bind(
            serverFd_,
            reinterpret_cast<const sockaddr*>(&address),
            sizeof(address)) == -1)
    {
        std::cerr << "[broker] bind failed: "
                  << std::strerror(errno) << '\n';

        stop();
        return false;
    }

    if (::listen(serverFd_, 8) == -1)
    {
        std::cerr << "[broker] listen failed: "
                  << std::strerror(errno) << '\n';

        stop();
        return false;
    }

    return true;
}

int UnixSocketServer::acceptClient() const
{
    while (true)
    {
        const int clientFd =
            ::accept(serverFd_, nullptr, nullptr);

        if (clientFd >= 0)
        {
            return clientFd;
        }

        if (errno != EINTR)
        {
            std::cerr << "[broker] accept failed: "
                      << std::strerror(errno) << '\n';

            return -1;
        }
    }
}

bool UnixSocketServer::receiveMessage(
    int clientFd,
    std::string& output)
{
    output.clear();

    char byte{};

    while (output.size() < ipc::MaximumMessageSize)
    {
        const auto bytesReceived =
            ::recv(clientFd, &byte, 1, 0);

        if (bytesReceived == 0)
        {
            return false;
        }

        if (bytesReceived == -1)
        {
            if (errno == EINTR)
            {
                continue;
            }

            std::cerr << "[broker] recv failed: "
                      << std::strerror(errno) << '\n';

            return false;
        }

        if (byte == '\n')
        {
            return true;
        }

        output.push_back(byte);
    }

    std::cerr << "[broker] incoming message exceeds limit\n";
    return false;
}

void UnixSocketServer::stop() noexcept
{
    if (serverFd_ != -1)
    {
        ::close(serverFd_);
        serverFd_ = -1;
    }

    if (!socketPath_.empty())
    {
        ::unlink(socketPath_.c_str());
    }
}