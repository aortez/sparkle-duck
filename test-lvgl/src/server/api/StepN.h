#pragma once

#include "ApiError.h"
#include "core/CommandWithCallback.h"
#include "core/Result.h"
#include <cstdint>
#include <nlohmann/json.hpp>

namespace DirtSim {
namespace Api {

namespace StepN {

struct Command {
    int frames = 1;

    nlohmann::json toJson() const;
    static Command fromJson(const nlohmann::json& j);
};

struct Okay {
    uint32_t timestep;

    nlohmann::json toJson() const;
};

using Response = Result<Okay, ApiError>;
using Cwc = CommandWithCallback<Command, Response>;

} // namespace StepN
} // namespace Api
} // namespace DirtSim
