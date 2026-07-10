#pragma once

#include <string_view>

namespace messaging::topics
{
    inline constexpr std::string_view MotionEvents
    {
        "motion.events"
    };

    inline constexpr std::string_view AnalyticsAlerts
    {
        "analytics.alerts"
    };
} // namespace messaging::topics

