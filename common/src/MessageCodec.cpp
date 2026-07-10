#include "messaging/MessageCodec.hpp"

#include <stdexcept>
#include <utility>

namespace messaging
{
    namespace
    {
        std::string toString(MessageType type)
        {
            switch (type)
            {
                case MessageType::Subscribe:
                    return "subscribe";

                case MessageType::Publish:
                    return "publish";

                case MessageType::Deliver:
                    return "deliver";

                case MessageType::Ack:
                    return "ack";

                case MessageType::Error:
                    return "error";
            }
            throw std::invalid_argument{"Unsupported message type"};
        }

        std::optional<MessageType> messageTypeFromString(std::string_view value)
        {
            if (value == "subscribe")
            {
                return MessageType::Subscribe;
            }

            if (value == "publish")
            {
                return MessageType::Publish;
            }

            if (value == "deliver")
            {
                return MessageType::Deliver;
            }

            if (value == "ack")
            {
                return MessageType::Ack;
            }

            if (value == "error")
            {
                return MessageType::Error;
            }

            return std::nullopt;
        }

        bool validate(const Message& message, std::string& error)
        {
            if (message.version != 1)
            {
                error = "Unsupported protocol schema version";
                return false;
            }

            if (message.clientId.empty())
            {
                error = "client_id is required";
                return false;
            }

            switch (message.type)
            {
                case MessageType::Subscribe:
                    if (message.topic.empty())
                    {
                        error = "subscribe message requires topic";
                        return false;
                    }
                    break;

                case MessageType::Publish:
                case MessageType::Deliver:
                    if (message.messageId.empty())
                    {
                        error = "message_id is required";
                        return false;
                    }

                    if (message.topic.empty())
                    {
                        error = "topic is required";
                        return false;
                    }
                    break;

                case MessageType::Ack:
                    if (message.messageId.empty())
                    {
                        error = "ack message requires message_id";
                        return false;
                    }
                    break;

                case MessageType::Error:
                    break;
            }

            return true;
        }
    }

    std::string MessageCodec::serialize(const Message& message)
    {
        nlohmann::json json{
            {"version", message.version},
            {"type", toString(message.type)},
            {"message_id", message.messageId},
            {"topic", message.topic},
            {"qos", static_cast<std::uint8_t>(message.qos)},
            {"client_id", message.clientId},
            {"payload", message.payload}
        };

        return json.dump();
    }

    std::optional<Message> MessageCodec::deserialize(std::string_view jsonText, std::string& error)
    {
        try
        {
            const auto json = nlohmann::json::parse(jsonText);

            if (!json.contains("version") ||
                !json.contains("type") ||
                !json.contains("client_id"))
            {
                error = "Message is missing required common fields";
                return std::nullopt;
            }

            const auto typeText = json.at("type").get<std::string>();
            const auto type = messageTypeFromString(typeText);

            if (!type.has_value())
            {
                error = "Unknown message type: " + typeText;
                return std::nullopt;
            }

            const auto qosValue = json.value("qos", 0);

            if (qosValue != 0 && qosValue != 1)
            {
                error = "qos must be either 0 or 1";
                return std::nullopt;
            }

            Message message{
                .version = json.at("version").get<std::uint32_t>(),
                .type = *type,
                .messageId = json.value("message_id", ""),
                .topic = json.value("topic", ""),
                .qos = static_cast<QoS>(qosValue),
                .clientId = json.at("client_id").get<std::string>(),
                .payload = json.value(
                    "payload",
                    nlohmann::json::object())
            };

            if (!validate(message, error))
            {
                return std::nullopt;
            }

            return message;
        }
        catch (const nlohmann::json::exception& exception)
        {
            error = exception.what();
            return std::nullopt;
        }
    }


} // namespace messaging