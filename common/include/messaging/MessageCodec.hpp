#pragma once

#include "messaging/Message.hpp"

#include <optional>
#include <string>
#include <string_view>

namespace messaging
{
    class MessageCodec
    {
        public:
            [[nodiscard]]
            static std::string serialize (const Message& message);

            [[nodiscard]]
            static std::optional<Message> deserialize (std::string_view jsonText, std::string& error);
    };

} // namespace messaging