#pragma once

#include "../../core/CommandWithCallback.h"
#include "../../core/MaterialType.h"
#include "../../core/Result.h"
#include "ApiError.h"
#include <variant>

namespace DirtSim {
namespace Api {

namespace CellSet {

struct Command {
    int x;
    int y;
    MaterialType material;
    double fill = 1.0;
};

using Response = Result<std::monostate, ApiError>;
using Cwc = CommandWithCallback<Command, Response>;

} // namespace CellSet
} // namespace Api
} // namespace DirtSim
