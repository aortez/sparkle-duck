#pragma once

#include "core/CommandWithCallback.h"
#include "core/Result.h"
#include "server/api/ApiError.h"
#include "server/api/ApiMacros.h"
#include <nlohmann/json.hpp>
#include <variant>

namespace DirtSim {
namespace UiApi {

namespace Exit {

DEFINE_API_NAME(Exit);

struct Command {
    API_COMMAND_NAME();
    nlohmann::json toJson() const;
    static Command fromJson(const nlohmann::json& j);
};

using OkayType = std::monostate;
using Response = Result<OkayType, ApiError>;
using Cwc = CommandWithCallback<Command, Response>;

} // namespace Exit
} // namespace UiApi
} // namespace DirtSim
