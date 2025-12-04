#pragma once

#include "ApiError.h"
#include "ApiMacros.h"
#include "core/CommandWithCallback.h"
#include "core/Result.h"
#include <nlohmann/json.hpp>
#include <zpp_bits.h>

namespace DirtSim {
namespace Api {
namespace PerfStatsGet {

DEFINE_API_NAME(PerfStatsGet);

struct Command {
    API_COMMAND_NAME();
    nlohmann::json toJson() const;
    static Command fromJson(const nlohmann::json& j);

    using serialize = zpp::bits::members<0>;
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

    API_COMMAND_NAME();
    nlohmann::json toJson() const;

    using serialize = zpp::bits::members<13>;
};

using OkayType = Okay;
using Response = Result<OkayType, ApiError>;
using Cwc = CommandWithCallback<Command, Response>;

} // namespace PerfStatsGet
} // namespace Api
} // namespace DirtSim
