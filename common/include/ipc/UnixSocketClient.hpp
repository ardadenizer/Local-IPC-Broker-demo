#pragma once

#include <string>
#include <string_view>

namespace ipc
{
    class UnixSocketClient
    {
    public:
        UnixSocketClient() = default;
        ~UnixSocketClient();

        UnixSocketClient(const UnixSocketClient&) = delete;
        UnixSocketClient& operator=(const UnixSocketClient&) = delete;

        UnixSocketClient(UnixSocketClient&& other) noexcept;
        UnixSocketClient& operator=(UnixSocketClient&& other) noexcept;

        [[nodiscard]]
        bool connectTo(std::string_view socketPath);

        [[nodiscard]]
        bool sendMessage(std::string_view message);

        [[nodiscard]]
        bool receiveMessage(std::string& messageLine, std::string& error);

        void disconnect() noexcept;

        [[nodiscard]]
        bool isConnected() const noexcept;

    private:
        int socketFd_{-1};
        std::string readBuffer_{};
    };

} // namespace ipc