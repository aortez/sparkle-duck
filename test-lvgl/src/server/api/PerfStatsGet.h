#pragma once

#include "ApiError.h"
#include "core/CommandWithCallback.h"
#include "core/Result.h"
#include <nlohmann/json.hpp>

namespace DirtSim {
namespace Api {
namespace PerfStatsGet {

struct Command {
    nlohmann::json toJson() const;
    static Command fromJson(const nlohmann::json& j);
};

struct Okay {
    double fps = 0.0;

    double physics_avg_ms = 0.0;
    double physics_total_ms = 0.0;
    uint32_t physics_calls = 0;

    double serialization_avg_ms = 0.0;
    double serialization_total_ms = 0.0;
    uint32_t serialization_calls = 0;

    double cache_update_avg_ms = 0.0;
    double cache_update_total_ms = 0.0;
    uint32_t cache_update_calls = 0;

    double network_send_avg_ms = 0.0;
    double network_send_total_ms = 0.0;
    uint32_t network_send_calls = 0;

    nlohmann::json toJson() const;
};

using Response = Result<Okay, ApiError>;
using Cwc = CommandWithCallback<Command, Response>;

} // namespace PerfStatsGet
} // namespace Api
} // namespace DirtSim
