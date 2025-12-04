#pragma once

#include "core/CommandWithCallback.h"
#include "core/Result.h"
#include "server/api/ApiError.h"
#include "server/api/ApiMacros.h"
#include <nlohmann/json.hpp>
#include <variant>

namespace DirtSim {
namespace UiApi {

namespace MouseUp {

DEFINE_API_NAME(MouseUp);

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

} // namespace MouseUp
} // namespace UiApi
} // namespace DirtSim
