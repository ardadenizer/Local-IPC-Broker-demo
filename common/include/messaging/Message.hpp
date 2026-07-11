#pragma once

#include <cstdint>
#include <string>

#include <nlohmann/json.hpp>

namespace messaging
{
    enum class MessageType
    {
        Subscribe,
        Publish,
        Deliver,
        Ack,
        Error
    };

    enum class QoS : std::uint8_t
    {
        AtMostOnce = 0, // volatile data
        AtLeastOnce = 1 // non-volatile data until ACK is received
    };

    struct Message
    {
        std::uint32_t version{1};

        MessageType type{MessageType::Publish};

        // Required for publish, deliver and ack messages.
        std::string messageId{};

        // Unix timestamp in seconds for publish and deliver messages.
        std::uint64_t timestamp{};

        // Required for subscribe, publish and deliver messages.
        std::string topic{};

        // Relevant primarily to publish and deliver messages.
        QoS qos{QoS::AtMostOnce};

        // Identifies the process communicating with the broker.
        std::string clientId{};

        // Topic-specific application data.
        nlohmann::json payload{nlohmann::json::object()};
    };
} // namespace messaging