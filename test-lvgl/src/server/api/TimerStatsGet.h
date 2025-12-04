#pragma once

#include "ApiError.h"
#include "ApiMacros.h"
#include "core/CommandWithCallback.h"
#include "core/Result.h"
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>

namespace DirtSim {
namespace Api {
namespace TimerStatsGet {

DEFINE_API_NAME(TimerStatsGet);

struct Command {
    API_COMMAND_NAME();
    nlohmann::json toJson() const;
    static Command fromJson(const nlohmann::json& j);
};

struct TimerEntry {
    double total_ms = 0.0;
    double avg_ms = 0.0;
    uint32_t calls = 0;
};

struct Okay {
    // Map of timer name -> stats.
    std::unordered_map<std::string, TimerEntry> timers;

    API_COMMAND_NAME();
    nlohmann::json toJson() const;
};

using OkayType = Okay;
using Response = Result<OkayType, ApiError>;
using Cwc = CommandWithCallback<Command, Response>;

} // namespace TimerStatsGet
} // namespace Api
} // namespace DirtSim
