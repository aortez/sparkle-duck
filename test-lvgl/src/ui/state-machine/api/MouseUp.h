#pragma once

#include "core/CommandWithCallback.h"
#include "core/Result.h"
#include "server/api/ApiError.h"
#include <nlohmann/json.hpp>
#include <variant>

namespace DirtSim {
namespace UiApi {

namespace MouseUp {

struct Command {
    int pixelX;
    int pixelY;

    nlohmann::json toJson() const;
    static Command fromJson(const nlohmann::json& j);
};

using Response = Result<std::monostate, ApiError>;
using Cwc = CommandWithCallback<Command, Response>;

} // namespace MouseUp
} // namespace UiApi
} // namespace DirtSim
