#include "MessageDispatcher.hpp"

#include <iostream>

void MessageDispatcher::handleMessage(const messaging::Message& message) const
{
    switch (message.type)
    {
        case messaging::MessageType::Publish:
            std::cout << "[broker] publish received\n"
                      << "  client: " << message.clientId << '\n'
                      << "  topic:  " << message.topic << '\n'
                      << "  id:     " << message.messageId << '\n';
            break;

        case messaging::MessageType::Subscribe:
            std::cout << "[broker] subscribe received\n"
                      << "  client: " << message.clientId << '\n'
                      << "  topic:  " << message.topic << '\n';
            break;

        case messaging::MessageType::Ack:
            std::cout << "[broker] acknowledgement received\n"
                      << "  client: " << message.clientId << '\n'
                      << "  id:     " << message.messageId << '\n';
            break;

        case messaging::MessageType::Deliver:
            std::cerr << "[broker] client cannot send deliver messages\n";
            break;

        case messaging::MessageType::Error:
            std::cerr << "[broker] error message received from client\n";
            break;
    }
}
