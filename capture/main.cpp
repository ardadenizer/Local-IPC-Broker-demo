#include "messaging/Message.hpp"
#include "messaging/MessageCodec.hpp"
#include "messaging/Topics.hpp"

#include <iostream>
#include <string>

int main()
{
    std::cout << "[Capture] is starting...\n";

    messaging::Message motionEvent{
        .version = 1,
        .type = messaging::MessageType::Publish,
        .messageId = "motion-1",
        .topic = std::string{messaging::topics::MotionEvents},
        .qos = messaging::QoS::AtMostOnce,
        .clientId = "capture",
        .payload = {
            {"event", "motion_detected"}
        }
    };

    const auto json =
        messaging::MessageCodec::serialize(motionEvent);

    std::cout << "[capture] generated message:\n"
              << json << std::endl;

  return 0;
}