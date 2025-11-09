#pragma once

#include "ApiError.h"
#include "core/CommandWithCallback.h"
#include "core/PhysicsSettings.h"
#include "core/Result.h"
#include <nlohmann/json.hpp>

namespace DirtSim {
namespace Api {
namespace PhysicsSettingsGet {

struct Command {
    nlohmann::json toJson() const;
    static Command fromJson(const nlohmann::json& j);
};

struct Okay {
    PhysicsSettings settings;

    nlohmann::json toJson() const;
};

using Response = Result<Okay, ApiError>;
using Cwc = CommandWithCallback<Command, Response>;

} // namespace PhysicsSettingsGet
} // namespace Api
} // namespace DirtSim
