#pragma once

#include "ApiError.h"
#include "core/CommandWithCallback.h"
#include "core/Result.h"
#include <cstdint>
#include <nlohmann/json.hpp>

namespace DirtSim {
namespace Api {

namespace SimRun {

struct Command {
    double timestep = 0.016;             // Default ~60 FPS.
    int max_steps = -1;                  // -1 = unlimited.
    std::string scenario_id = "sandbox"; // Scenario to run (default: sandbox).
    bool use_realtime =
        true; // True: real-time accumulation, False: force immediate steps (for testing).
        // TODO: replace with an enum class( enum class RunMode::RealTime, etc....)

    nlohmann::json toJson() const;
    static Command fromJson(const nlohmann::json& j);
};

struct Okay {
    bool running;
    uint32_t current_step;

    nlohmann::json toJson() const;
};

using Response = Result<Okay, ApiError>;
using Cwc = CommandWithCallback<Command, Response>;

} // namespace SimRun
} // namespace Api
} // namespace DirtSim
