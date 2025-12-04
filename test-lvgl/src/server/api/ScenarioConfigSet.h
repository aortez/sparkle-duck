#pragma once

#include "ApiError.h"
#include "ApiMacros.h"
#include "core/CommandWithCallback.h"
#include "core/Result.h"
#include "core/ScenarioConfig.h"
#include <string>
#include <zpp_bits.h>

namespace DirtSim {
namespace Api {
namespace ScenarioConfigSet {

DEFINE_API_NAME(ScenarioConfigSet);

/**
 * @brief Command to update scenario configuration.
 */
struct Command {
    ScenarioConfig config; // New configuration to apply.

    API_COMMAND_NAME();
    nlohmann::json toJson() const;
    static Command fromJson(const nlohmann::json& j);

    using serialize = zpp::bits::members<1>;
};

/**
 * @brief Success response.
 */
struct Okay {
    bool success;

    API_COMMAND_NAME();
    nlohmann::json toJson() const;

    using serialize = zpp::bits::members<1>;
};

using OkayType = Okay;
using Response = Result<OkayType, ApiError>;
using Cwc = CommandWithCallback<Command, Response>;

} // namespace ScenarioConfigSet
} // namespace Api
} // namespace DirtSim
