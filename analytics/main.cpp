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

    std::cout << "[analytics] is starting...\n";

    client::BrokerClient brokerClient{std::string{ipc::BrokerSocketPath}};

    if (!brokerClient.connect())
    {
        std::cerr << "[analytics] failed to connect to broker\n";
        return 1;
    }

    if (!brokerClient.subscribe(messaging::topics::MotionEvents, "analytics"))
    {
        std::cerr << "[analytics] subscribe failed\n";
        return 1;
    }

    std::cout << "[analytics] subscribed topic=" << messaging::topics::MotionEvents << '\n';

    std::uint64_t decisionSequence = 1;

    while (keepRunning)
    {
        std::string error;
        const auto inbound = brokerClient.receive(error);
        if (!inbound)
        {
            std::cerr << "[analytics] receive failed: " << error << '\n';
            return 1;
        }

        std::cout << "[analytics] inbound frame: "
                  << messaging::MessageCodec::serialize(*inbound)
                  << '\n';

        if (inbound->type != messaging::MessageType::Deliver ||
            inbound->topic != messaging::topics::MotionEvents)
        {
            continue;
        }

        const std::string event = inbound->payload.value("event", "unknown");
        if (event != "motion_detected")
        {
            std::cout << "[analytics] decision=no_alert source=" << inbound->messageId
                      << " event=" << event << '\n';
            continue;
        }

        const auto now = std::chrono::system_clock::now();
        const auto timestamp = static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::seconds>(
                now.time_since_epoch())
                .count());

        messaging::Message analyticsEvent{
            .version = 1,
            .type = messaging::MessageType::Publish,
            .messageId = "analytics-" + std::to_string(decisionSequence),
            .timestamp = timestamp,
            .topic = std::string{messaging::topics::AnalyticsAlerts},
            .qos = messaging::QoS::AtLeastOnce,
            .clientId = "analytics",
            .payload = {
                {"event", "critical_message"},
                {"decision", "alert"},
                {"source_message_id", inbound->messageId},
                {"source_client", inbound->clientId}
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

        std::cout << "[analytics] decision=alert published id=" << analyticsEvent.messageId
                  << " source=" << inbound->messageId << '\n';
        ++decisionSequence;
    }

    std::cout << "[analytics] shutdown requested, stopping processor\n";

    return 0;
}