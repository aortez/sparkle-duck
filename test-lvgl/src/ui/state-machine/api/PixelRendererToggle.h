#pragma once

#include "core/CommandWithCallback.h"
#include "core/Result.h"
#include "server/api/ApiError.h"
#include <nlohmann/json.hpp>

namespace DirtSim {
namespace UiApi {

namespace PixelRendererToggle {

struct Command {
    bool enabled;

    nlohmann::json toJson() const;
    static Command fromJson(const nlohmann::json& j);
};

struct Okay {
    bool enabled;

    nlohmann::json toJson() const;
    static Okay fromJson(const nlohmann::json& j);
};

using Response = Result<Okay, ApiError>;
using Cwc = CommandWithCallback<Command, Response>;

} // namespace PixelRendererToggle
} // namespace UiApi
} // namespace DirtSim
