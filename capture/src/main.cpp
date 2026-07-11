#include "messaging/Message.hpp"
#include "messaging/MessageCodec.hpp"
#include "messaging/Topics.hpp"
#include "client/BrokerClient.hpp"
#include "ipc/SocketConfig.hpp"

#include <atomic>
#include <chrono>
#include <csignal>
#include <iostream>
#include <string>
#include <thread>

namespace
{
std::atomic<bool> keepRunning{true};

void handleSignal(int)
{
    keepRunning = false;
}
}

int main()
{
    std::signal(SIGINT, handleSignal);
    std::signal(SIGTERM, handleSignal);

    std::cout << "[capture] is starting continuous stream...\n";

    client::BrokerClient brokerClient{std::string{ipc::BrokerSocketPath}};

    if (!brokerClient.connect())
    {
        std::cerr << "[capture] failed to connect to broker\n";
        return 1;
    }

    std::uint64_t sequence = 1;
    bool motionDetected = true;

    while (keepRunning)
    {
        const auto now = std::chrono::system_clock::now();
        const auto timestamp = static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::seconds>(
                now.time_since_epoch())
                .count());

        const std::string event = motionDetected ? "motion_detected" : "no_motion";
        const std::string messageId = "motion-" + std::to_string(sequence);

        messaging::Message motionEvent{
            .version = 1,
            .type = messaging::MessageType::Publish,
            .messageId = messageId,
            .timestamp = timestamp,
            .topic = std::string{messaging::topics::MotionEvents},
            .qos = messaging::QoS::AtMostOnce,
            .clientId = "capture",
            .payload = {
                {"event", event},
                {"sequence", sequence}
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

        std::cout << "[capture] motion event published id=" << messageId
                  << " event=" << event << std::endl;

        ++sequence;
        motionDetected = !motionDetected;
        std::this_thread::sleep_for(std::chrono::milliseconds{400});
    }

    std::cout << "[capture] shutdown requested, stopping stream\n";

    return 0;
}