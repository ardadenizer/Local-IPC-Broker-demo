#include "ipc/UnixSocketClient.hpp"

#include <cerrno>
#include <chrono>
#include <cstring>
#include <iostream>
#include <thread>
#include <utility>

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

namespace ipc
{
namespace
{

bool sendAll(int socketFd, std::string_view data)
{
    std::size_t totalSent{0};

    while (totalSent < data.size())
    {
        const auto bytesSent = ::send(
            socketFd,
            data.data() + totalSent,
            data.size() - totalSent,
            MSG_NOSIGNAL);

        if (bytesSent > 0)
        {
            totalSent += static_cast<std::size_t>(bytesSent);
            continue;
        }

        if (bytesSent == -1 && errno == EINTR)
        {
            continue;
        }

        std::cerr << "[socket-client] send failed: "
                  << std::strerror(errno) << '\n';

        return false;
    }

    return true;
}

} // namespace

UnixSocketClient::~UnixSocketClient()
{
    disconnect();
}

UnixSocketClient::UnixSocketClient(
    UnixSocketClient&& other) noexcept
    : socketFd_{std::exchange(other.socketFd_, -1)}
{
}

UnixSocketClient& UnixSocketClient::operator=(
    UnixSocketClient&& other) noexcept
{
    if (this != &other)
    {
        disconnect();
        socketFd_ = std::exchange(other.socketFd_, -1);
    }

    return *this;
}

bool UnixSocketClient::connectTo(std::string_view socketPath)
{
    disconnect();

    if (socketPath.size() >= sizeof(sockaddr_un::sun_path))
    {
        std::cerr << "[socket-client] socket path is too long\n";
        return false;
    }

    sockaddr_un address{};
    address.sun_family = AF_UNIX;

    std::memcpy(
        address.sun_path,
        socketPath.data(),
        socketPath.size());

    address.sun_path[socketPath.size()] = '\0';

    constexpr int maxAttempts = 20;
    constexpr auto retryDelay = std::chrono::milliseconds{100};

    for (int attempt = 1; attempt <= maxAttempts; ++attempt)
    {
        socketFd_ = ::socket(AF_UNIX, SOCK_STREAM, 0);

        if (socketFd_ == -1)
        {
            std::cerr << "[socket-client] socket creation failed: "
                      << std::strerror(errno) << '\n';
            return false;
        }

        if (::connect(
                socketFd_,
                reinterpret_cast<const sockaddr*>(&address),
                sizeof(address)) == 0)
        {
            return true;
        }

        const int connectErrno = errno;
        disconnect();

        if (connectErrno != ENOENT && connectErrno != ECONNREFUSED)
        {
            std::cerr << "[socket-client] connect failed: "
                      << std::strerror(connectErrno) << '\n';
            return false;
        }

        if (attempt < maxAttempts)
        {
            std::this_thread::sleep_for(retryDelay);
            continue;
        }

        std::cerr << "[socket-client] connect failed: "
                  << std::strerror(connectErrno) << '\n';
        return false;
    }

    return false;
}

bool UnixSocketClient::sendMessage(std::string_view message)
{
    if (!isConnected())
    {
        std::cerr << "[socket-client] client is not connected\n";
        return false;
    }

    if (!sendAll(socketFd_, message))
    {
        return false;
    }

    return sendAll(socketFd_, "\n");
}

bool UnixSocketClient::receiveMessage(std::string& messageLine, std::string& error)
{
    messageLine.clear();

    if (!isConnected())
    {
        error = "client is not connected";
        return false;
    }

    while (true)
    {
        const std::size_t newlinePos = readBuffer_.find('\n');
        if (newlinePos != std::string::npos)
        {
            messageLine = readBuffer_.substr(0, newlinePos);
            readBuffer_.erase(0, newlinePos + 1);

            if (!messageLine.empty() && messageLine.back() == '\r')
            {
                messageLine.pop_back();
            }

            return true;
        }

        char buffer[512];
        const ssize_t bytesRead = ::recv(socketFd_, buffer, sizeof(buffer), 0);
        if (bytesRead > 0)
        {
            readBuffer_.append(buffer, static_cast<std::size_t>(bytesRead));
            continue;
        }

        if (bytesRead == 0)
        {
            error = "socket closed by peer";
            return false;
        }

        if (errno == EINTR)
        {
            continue;
        }

        error = std::string{"recv failed: "} + std::strerror(errno);
        return false;
    }
}

void UnixSocketClient::disconnect() noexcept
{
    if (socketFd_ != -1)
    {
        ::close(socketFd_);
        socketFd_ = -1;
    }

    readBuffer_.clear();
}

bool UnixSocketClient::isConnected() const noexcept
{
    return socketFd_ != -1;
}

} // namespace ipc