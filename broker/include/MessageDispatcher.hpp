#pragma once

#include "messaging/Message.hpp"

class MessageDispatcher
{
public:
    void handleMessage(const messaging::Message& message) const;
};
