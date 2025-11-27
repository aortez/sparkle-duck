#pragma once

#include "ApiError.h"
#include "ApiMacros.h"
#include "core/CommandWithCallback.h"
#include "core/RenderMessage.h"
#include "core/Result.h"
#include <nlohmann/json.hpp>
#include <variant>

namespace DirtSim {
namespace Api {

namespace RenderFormatSet {

DEFINE_API_NAME(RenderFormatSet);

struct Command {
    RenderFormat format;

    API_COMMAND_NAME();
    nlohmann::json toJson() const;
    static Command fromJson(const nlohmann::json& j);
};

struct Okay {
    RenderFormat active_format;
    std::string message;

    API_COMMAND_NAME();
    nlohmann::json toJson() const;
};

using Response = Result<Okay, ApiError>;
using Cwc = CommandWithCallback<Command, Response>;

} // namespace RenderFormatSet
} // namespace Api
} // namespace DirtSim
