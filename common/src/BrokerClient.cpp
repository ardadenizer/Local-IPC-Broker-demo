#include "client/BrokerClient.hpp"

#include "messaging/MessageCodec.hpp"

#include <optional>

#include <utility>

namespace client
{

BrokerClient::BrokerClient(std::string socketPath)
    : socketPath_{std::move(socketPath)}
{
}

bool BrokerClient::connect()
{
    return socketClient_.connectTo(socketPath_);
}

bool BrokerClient::publish(const messaging::Message& message)
{
    const auto encodedMessage = messaging::MessageCodec::serialize(message);

    return socketClient_.sendMessage(encodedMessage);
}

bool BrokerClient::subscribe(std::string_view topic, std::string_view clientId)
{
    messaging::Message subscribeMessage{
        .version = 1,
        .type = messaging::MessageType::Subscribe,
        .messageId = "",
        .timestamp = 0,
        .topic = std::string{topic},
        .qos = messaging::QoS::AtMostOnce,
        .clientId = std::string{clientId},
        .payload = nlohmann::json::object()
    };

    return publish(subscribeMessage);
}

std::optional<messaging::Message> BrokerClient::receive(std::string& error)
{
    std::string messageLine;
    if (!socketClient_.receiveMessage(messageLine, error))
    {
        return std::nullopt;
    }

    return messaging::MessageCodec::deserialize(messageLine, error);
}

void BrokerClient::disconnect() noexcept
{
    socketClient_.disconnect();
}

bool BrokerClient::isConnected() const noexcept
{
    return socketClient_.isConnected();
}

} // namespace client
