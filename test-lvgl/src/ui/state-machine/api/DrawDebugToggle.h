#pragma once

#include "core/CommandWithCallback.h"
#include "core/Result.h"
#include "server/api/ApiError.h"
#include "server/api/ApiMacros.h"
#include <nlohmann/json.hpp>

namespace DirtSim {
namespace UiApi {

namespace DrawDebugToggle {

DEFINE_API_NAME(DrawDebugToggle);

struct Command {
    bool enabled;

    API_COMMAND_NAME();
    nlohmann::json toJson() const;
    static Command fromJson(const nlohmann::json& j);
};

struct Okay {
    bool enabled;

    API_COMMAND_NAME();
    nlohmann::json toJson() const;
    static Okay fromJson(const nlohmann::json& j);
};

using OkayType = Okay;
using Response = Result<OkayType, ApiError>;
using Cwc = CommandWithCallback<Command, Response>;

} // namespace DrawDebugToggle
} // namespace UiApi
} // namespace DirtSim
