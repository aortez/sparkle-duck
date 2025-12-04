#pragma once

#include "core/CommandWithCallback.h"
#include "core/Result.h"
#include "server/api/ApiError.h"
#include "server/api/ApiMacros.h"
#include <nlohmann/json.hpp>

namespace DirtSim {
namespace UiApi {

namespace SimPause {

DEFINE_API_NAME(SimPause);

struct Command {
    API_COMMAND_NAME();
    nlohmann::json toJson() const;
    static Command fromJson(const nlohmann::json& j);
};

struct Okay {
    bool paused;

    API_COMMAND_NAME();
    nlohmann::json toJson() const;
};

using OkayType = Okay;
using Response = Result<OkayType, ApiError>;
using Cwc = CommandWithCallback<Command, Response>;

} // namespace SimPause
} // namespace UiApi
} // namespace DirtSim
