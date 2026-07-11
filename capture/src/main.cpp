#include "messaging/Message.hpp"
#include "messaging/MessageCodec.hpp"
#include "messaging/Topics.hpp"
#include "client/BrokerClient.hpp"
#include "ipc/SocketConfig.hpp"

#include <iostream>
#include <string>

int main()
{
    std::cout << "[Capture] is starting...\n";

    client::BrokerClient brokerClient{std::string{ipc::BrokerSocketPath}};

    if (!brokerClient.connect())
    {
        std::cerr << "[capture] failed to connect to broker\n";
        return 1;
    }

    messaging::Message motionEvent{
        .version = 1,
        .type = messaging::MessageType::Publish,
        .messageId = "motion-001",
        .timestamp = 1720000000,
        .topic = std::string{messaging::topics::MotionEvents},
        .qos = messaging::QoS::AtMostOnce,
        .clientId = "capture",
        .payload = {
            {"event", "motion_detected"}
        }
    };

    std::cout << "[capture] outbound publish frame: "
              << messaging::MessageCodec::serialize(motionEvent)
              << '\n';

    if (!brokerClient.publish(motionEvent))
    {
        std::cerr << "[capture] publish failed\n";
        return 1;
    }

    std::cout << "[capture] motion event published" << std::endl;

  return 0;
}