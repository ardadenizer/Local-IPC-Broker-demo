#include "messaging/Message.hpp"
#include "messaging/MessageCodec.hpp"
#include "messaging/Topics.hpp"
#include "client/BrokerClient.hpp"
#include "ipc/SocketConfig.hpp"

#include <iostream>
#include <string>

int main()
{
  std::cout << "[analytics] is starting...\n" ;

   client::BrokerClient brokerClient{std::string{ipc::BrokerSocketPath}};

    if (!brokerClient.connect())
    {
        std::cerr << "[analytics] failed to connect to broker\n";
        return 1;
    }

    messaging::Message analyticsEvent{
        .version = 1,
        .type = messaging::MessageType::Publish,
        .messageId = "analytics-001",
        .timestamp = 1720000001,
        .topic = std::string{messaging::topics::AnalyticsAlerts},
        .qos = messaging::QoS::AtLeastOnce,
        .clientId = "analytics",
        .payload = {
            {"event", "critical_message"}
        }
    };

    std::cout << "[analytics] outbound publish frame: "
              << messaging::MessageCodec::serialize(analyticsEvent)
              << '\n';

    if (!brokerClient.publish(analyticsEvent))
    {
        std::cerr << "[analytics] publish failed\n";
        return 1;
    }

    std::cout << "[analytics] analytics event published" << std::endl;

  return 0;
}