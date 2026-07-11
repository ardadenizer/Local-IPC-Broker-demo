#pragma once

#include "client/IBrokerClient.hpp"
#include "ipc/UnixSocketClient.hpp"

#include <optional>
#include <string>
#include <string_view>

namespace client
{

    class BrokerClient final : public IBrokerClient
    {
    public:
        explicit BrokerClient(std::string socketPath);

        [[nodiscard]]
        bool connect() override;

        [[nodiscard]]
        bool publish(
            const messaging::Message& message) override;

        [[nodiscard]]
        bool subscribe(std::string_view topic, std::string_view clientId) override;

        [[nodiscard]]
        std::optional<messaging::Message> receive(std::string& error) override;

        void disconnect() noexcept override;

        [[nodiscard]]
        bool isConnected() const noexcept override;

    private:
        std::string socketPath_;
        ipc::UnixSocketClient socketClient_;
    };

} // namespace client