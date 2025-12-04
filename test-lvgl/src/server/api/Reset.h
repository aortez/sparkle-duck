#pragma once

#include "ApiError.h"
#include "ApiMacros.h"
#include "core/CommandWithCallback.h"
#include "core/Result.h"
#include <nlohmann/json.hpp>
#include <variant>

namespace DirtSim {
namespace Api {

namespace Reset {

DEFINE_API_NAME(Reset);

struct Command {
    API_COMMAND_NAME();
    nlohmann::json toJson() const;
    static Command fromJson(const nlohmann::json& j);
};

using OkayType = std::monostate;
using Response = Result<OkayType, ApiError>;
using Cwc = CommandWithCallback<Command, Response>;

} // namespace Reset
} // namespace Api
} // namespace DirtSim
