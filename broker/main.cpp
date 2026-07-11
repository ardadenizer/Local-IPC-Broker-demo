#include "messaging/MessageCodec.hpp"

#include <iostream>
#include <string>

int main()
{
  std::cout << "[Broker] is starting...\n";

  const std::string incoming = R"({
        "version": 1,
        "type": "publish",
      "message_id": "motion-001",
      "timestamp": 1720000000,
        "topic": "motion.events",
        "qos": 0,
        "client_id": "capture",
        "payload": {
            "event": "motion_detected"
        }
    })";

    std::string error {};

    const auto message = messaging::MessageCodec::deserialize(incoming, error);

    if (!message)
    {
        std::cerr << "[broker] invalid message: "
                  << error << '\n';
        return 1;
    }

    std::cout << "[broker] received topic: "
              << message->topic << '\n';

    std::cout << "[broker] received from: "
              << message->clientId << std::endl;

        std::cout << "[broker] received timestamp: "
                            << message->timestamp << std::endl;

  return 0;
}