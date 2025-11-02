#pragma once

#include "ApiError.h"
#include "../../core/CommandWithCallback.h"
#include "../../core/Result.h"

namespace DirtSim {
namespace Api {

namespace GravitySet {

struct Command {
    double gravity;
};

using Response = Result<std::monostate, ApiError>;
using Cwc = CommandWithCallback<Command, Response>;

} // namespace GravitySet
} // namespace Api
} // namespace DirtSim
