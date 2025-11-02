#pragma once

#include "ApiError.h"
#include "../../core/Cell.h"
#include "../../core/CommandWithCallback.h"
#include "../../core/Result.h"

namespace DirtSim {
namespace Api {

namespace CellGet {

struct Command {
    int x;
    int y;
};

struct Okay {
    Cell cell;
};

using Response = Result<Okay, ApiError>;
using Cwc = CommandWithCallback<Command, Response>;

} // namespace CellGet
} // namespace Api
} // namespace DirtSim
