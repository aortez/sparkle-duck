#pragma once

#include "ApiError.h"
#include "core/CommandWithCallback.h"
#include "core/Result.h"
#include "core/WorldData.h"
#include <nlohmann/json.hpp>

namespace DirtSim {
namespace Api {

namespace StateGet {

struct Command {
    nlohmann::json toJson() const;
    static Command fromJson(const nlohmann::json& j);
};

struct Okay {
    WorldData worldData;  // Changed from World to WorldData.

    nlohmann::json toJson() const;
};

using Response = Result<Okay, ApiError>;
using Cwc = CommandWithCallback<Command, Response>;

} // namespace StateGet
} // namespace Api
} // namespace DirtSim
