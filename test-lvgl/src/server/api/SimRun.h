#pragma once

#include "ApiError.h"
#include "ApiMacros.h"
#include "core/CommandWithCallback.h"
#include "core/Result.h"
#include <cstdint>
#include <nlohmann/json.hpp>
#include <zpp_bits.h>

namespace DirtSim {
namespace Api {

namespace SimRun {

DEFINE_API_NAME(SimRun);

struct Command {
    double timestep = 0.016;             // Default ~60 FPS.
    int max_steps = -1;                  // -1 = unlimited.
    std::string scenario_id = "sandbox"; // Scenario to run (default: sandbox).
    int max_frame_ms =
        0; // Max milliseconds per frame. 0 = unlimited (as fast as possible), >0 = frame rate cap.

    API_COMMAND_NAME();
    nlohmann::json toJson() const;
    static Command fromJson(const nlohmann::json& j);

    using serialize = zpp::bits::members<4>;
};

struct Okay {
    bool running;
    uint32_t current_step;

    API_COMMAND_NAME();
    nlohmann::json toJson() const;

    using serialize = zpp::bits::members<2>;
};

using OkayType = Okay;
using Response = Result<OkayType, ApiError>;
using Cwc = CommandWithCallback<Command, Response>;

} // namespace SimRun
} // namespace Api
} // namespace DirtSim
