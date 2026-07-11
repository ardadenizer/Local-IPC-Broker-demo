#pragma once

#include "messaging/Message.hpp"

#include <optional>
#include <string_view>

namespace client
{

    class IBrokerClient
    {
    public:
        virtual ~IBrokerClient() = default;

        [[nodiscard]]
        virtual bool connect() = 0;

        [[nodiscard]]
        virtual bool publish(const messaging::Message& message) = 0;

        [[nodiscard]]
        virtual bool subscribe(std::string_view topic, std::string_view clientId) = 0;

        [[nodiscard]]
        virtual std::optional<messaging::Message> receive(std::string& error) = 0;

        virtual void disconnect() noexcept = 0;

        [[nodiscard]]
        virtual bool isConnected() const noexcept = 0;
    };

} // namespace client