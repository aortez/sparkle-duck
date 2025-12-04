#pragma once

#include "core/CommandWithCallback.h"
#include "core/Result.h"
#include "server/api/ApiError.h"
#include "server/api/ApiMacros.h"
#include <nlohmann/json.hpp>
#include <string>
#include <variant>

namespace DirtSim {
namespace UiApi {

namespace ScreenGrab {

DEFINE_API_NAME(ScreenGrab);

struct Command {
    double scale = 1.0; // Resolution scale factor (0.25 = 4x smaller, 1.0 = full res).

    API_COMMAND_NAME();
    nlohmann::json toJson() const;
    static Command fromJson(const nlohmann::json& j);
};

struct Okay {
    std::string pixels; // Base64-encoded raw ARGB8888 pixel data.
    uint32_t width;
    uint32_t height;

    API_COMMAND_NAME();
    nlohmann::json toJson() const;
};

using OkayType = Okay;
using Response = Result<OkayType, ApiError>;
using Cwc = CommandWithCallback<Command, Response>;

} // namespace ScreenGrab
} // namespace UiApi
} // namespace DirtSim
