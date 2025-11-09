#pragma once

#include "ApiError.h"
#include "core/CommandWithCallback.h"
#include "core/Result.h"
#include <nlohmann/json.hpp>
#include <variant>

namespace DirtSim {
namespace Api {
namespace SpawnDirtBall {

struct Command {
    // No parameters needed - just spawn a dirt ball at the default location.

    nlohmann::json toJson() const;
    static Command fromJson(const nlohmann::json& j);
};

using Response = Result<std::monostate, ApiError>;
using Cwc = CommandWithCallback<Command, Response>;

} // namespace SpawnDirtBall
} // namespace Api
} // namespace DirtSim
