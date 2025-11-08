#pragma once

#include "core/CommandWithCallback.h"
#include "core/Result.h"
#include "server/api/ApiError.h"
#include <nlohmann/json.hpp>

namespace DirtSim {
namespace UiApi {

namespace SimPause {

struct Command {
    nlohmann::json toJson() const;
    static Command fromJson(const nlohmann::json& j);
};

struct Okay {
    bool paused;

    nlohmann::json toJson() const;
};

using Response = Result<Okay, ApiError>;
using Cwc = CommandWithCallback<Command, Response>;

} // namespace SimPause
} // namespace UiApi
} // namespace DirtSim
