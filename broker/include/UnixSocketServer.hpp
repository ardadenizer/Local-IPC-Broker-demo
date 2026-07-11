#pragma once

#include <string>
#include <string_view>

class UnixSocketServer
{
public:
    explicit UnixSocketServer(std::string socketPath);
    ~UnixSocketServer();

    UnixSocketServer(const UnixSocketServer&) = delete;
    UnixSocketServer& operator=(const UnixSocketServer&) = delete;

    [[nodiscard]]
    bool start();

    [[nodiscard]]
    int acceptClient() const;

    [[nodiscard]]
    int listeningFd() const noexcept;

    [[nodiscard]]
    static bool receiveMessage(int clientFd, std::string& output);

private:
    void stop() noexcept;

    std::string socketPath_;
    int serverFd_{-1};
};