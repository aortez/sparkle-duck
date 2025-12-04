#pragma once

#include "core/CommandWithCallback.h"
#include "core/Result.h"
#include "server/api/ApiError.h"
#include "server/api/ApiMacros.h"
#include "ui/rendering/RenderMode.h"
#include <nlohmann/json.hpp>

namespace DirtSim {
namespace UiApi {

namespace RenderModeSelect {

DEFINE_API_NAME(RenderModeSelect);

struct Command {
    Ui::RenderMode mode;

    API_COMMAND_NAME();
    nlohmann::json toJson() const;
    static Command fromJson(const nlohmann::json& j);
};

struct Okay {
    Ui::RenderMode mode;

    API_COMMAND_NAME();
    nlohmann::json toJson() const;
    static Okay fromJson(const nlohmann::json& j);
};

using OkayType = Okay;
using Response = Result<OkayType, ApiError>;
using Cwc = CommandWithCallback<Command, Response>;

} // namespace RenderModeSelect
} // namespace UiApi
} // namespace DirtSim
