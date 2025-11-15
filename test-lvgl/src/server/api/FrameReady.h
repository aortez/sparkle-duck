#pragma once

#include "ApiError.h"
#include "ApiMacros.h"
#include "core/CommandWithCallback.h"
#include "core/Result.h"
#include <nlohmann/json.hpp>
#include <variant>

namespace DirtSim {
namespace Api {

namespace FrameReady {

DEFINE_API_NAME(FrameReady);

struct Command {
    API_COMMAND_NAME();
    nlohmann::json toJson() const;
    static Command fromJson(const nlohmann::json& j);
};

using Response = Result<std::monostate, ApiError>;
using Cwc = CommandWithCallback<Command, Response>;

} // namespace FrameReady
} // namespace Api
} // namespace DirtSim
