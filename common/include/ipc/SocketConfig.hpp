#pragma once

#include <cstddef>
#include <string_view>

namespace ipc
{

    inline constexpr std::string_view BrokerSocketPath{
        "/tmp/ipc_broker.sock"
    };

    inline constexpr std::size_t MaximumMessageSize{
        4096
    };

} // namespace ipc