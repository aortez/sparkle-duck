#pragma once

#include "core/CommandWithCallback.h"
#include "core/Result.h"
#include "core/ScenarioConfig.h"
#include "ApiError.h"
#include <string>

namespace DirtSim {
namespace Api {
namespace ScenarioConfigSet {

/**
 * @brief Command to update scenario configuration.
 */
struct Command {
    ScenarioConfig config;  // New configuration to apply.

    nlohmann::json toJson() const;
    static Command fromJson(const nlohmann::json& j);
    static constexpr const char* name() { return "scenario_config_set"; }
};

/**
 * @brief Success response.
 */
struct Okay {
    bool success;

    nlohmann::json toJson() const;
    static constexpr const char* name() { return "ScenarioConfigSet::Okay"; }
};

using Response = Result<Okay, ApiError>;
using Cwc = CommandWithCallback<Command, Response>;

} // namespace ScenarioConfigSet
} // namespace Api
} // namespace DirtSim
