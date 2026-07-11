#include "client/BrokerClient.hpp"

#include "messaging/MessageCodec.hpp"

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

void BrokerClient::disconnect() noexcept
{
    socketClient_.disconnect();
}

bool BrokerClient::isConnected() const noexcept
{
    return socketClient_.isConnected();
}

} // namespace client
