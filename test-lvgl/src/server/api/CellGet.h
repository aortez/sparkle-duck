#pragma once

#include "ApiError.h"
#include "ApiMacros.h"
#include "core/Cell.h"
#include "core/CommandWithCallback.h"
#include "core/Result.h"
#include <nlohmann/json.hpp>

namespace DirtSim {
namespace Api {

namespace CellGet {

DEFINE_API_NAME(CellGet);

struct Command {
    int x;
    int y;

    API_COMMAND_NAME();
    nlohmann::json toJson() const;
    static Command fromJson(const nlohmann::json& j);
};

struct Okay {
    Cell cell;

    API_COMMAND_NAME();
    nlohmann::json toJson() const;
};

using OkayType = Okay;
using Response = Result<OkayType, ApiError>;
using Cwc = CommandWithCallback<Command, Response>;

} // namespace CellGet
} // namespace Api
} // namespace DirtSim
