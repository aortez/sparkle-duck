#pragma once

#include "ApiError.h"
#include "../../core/CommandWithCallback.h"
#include "../../core/Result.h"
#include <cstdint>

namespace DirtSim {
namespace Api {

namespace StepN {

struct Command {
    int frames = 1;
};

struct Okay {
    uint32_t timestep;
};

using Response = Result<Okay, ApiError>;
using Cwc = CommandWithCallback<Command, Response>;

} // namespace StepN
} // namespace Api
} // namespace DirtSim
