#pragma once

#include "ApiError.h"
#include "../../core/CommandWithCallback.h"
#include "../../core/Result.h"
#include "../../core/World.h"

namespace DirtSim {
namespace Api {

namespace StateGet {

struct Command {
};

struct Okay {
    World world;
};

using Response = Result<Okay, ApiError>;
using Cwc = CommandWithCallback<Command, Response>;

} // namespace StateGet
} // namespace Api
} // namespace DirtSim
