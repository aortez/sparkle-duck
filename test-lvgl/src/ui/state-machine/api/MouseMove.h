#pragma once

#include "core/CommandWithCallback.h"
#include "core/Result.h"
#include "server/api/ApiError.h"
#include "server/api/ApiMacros.h"
#include <nlohmann/json.hpp>
#include <variant>

namespace DirtSim {
namespace UiApi {

namespace MouseMove {

DEFINE_API_NAME(MouseMove);

struct Command {
    int pixelX;
    int pixelY;

    API_COMMAND_NAME();
    nlohmann::json toJson() const;
    static Command fromJson(const nlohmann::json& j);
};

using OkayType = std::monostate;
using Response = Result<OkayType, ApiError>;
using Cwc = CommandWithCallback<Command, Response>;

} // namespace MouseMove
} // namespace UiApi
} // namespace DirtSim
