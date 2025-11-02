#pragma once

#include "../../core/CommandWithCallback.h"
#include "../../core/Result.h"
#include "ApiError.h"
#include <variant>

namespace DirtSim {
namespace Api {

namespace Reset {

struct Command {
};

using Response = Result<std::monostate, ApiError>;
using Cwc = CommandWithCallback<Command, Response>;

} // namespace Reset
} // namespace Api
} // namespace DirtSim
