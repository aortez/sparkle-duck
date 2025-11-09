#pragma once

#include "ApiError.h"
#include "core/CommandWithCallback.h"
#include "core/Result.h"
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>

namespace DirtSim {
namespace Api {
namespace TimerStatsGet {

struct Command {
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

    nlohmann::json toJson() const;
};

using Response = Result<Okay, ApiError>;
using Cwc = CommandWithCallback<Command, Response>;

} // namespace TimerStatsGet
} // namespace Api
} // namespace DirtSim
