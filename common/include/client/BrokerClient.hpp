#pragma once

#include "client/IBrokerClient.hpp"
#include "ipc/UnixSocketClient.hpp"

#include <string>

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

        void disconnect() noexcept override;

        [[nodiscard]]
        bool isConnected() const noexcept override;

    private:
        std::string socketPath_;
        ipc::UnixSocketClient socketClient_;
    };

} // namespace client