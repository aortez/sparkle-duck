#pragma once

#include "ApiError.h"
#include "ApiMacros.h"
#include "core/CommandWithCallback.h"
#include "core/MaterialType.h"
#include "core/Result.h"
#include <nlohmann/json.hpp>
#include <variant>

namespace DirtSim {
namespace Api {

namespace CellSet {

DEFINE_API_NAME(CellSet);

struct Command {
    int x;
    int y;
    MaterialType material;
    double fill = 1.0;

    API_COMMAND_NAME();
    nlohmann::json toJson() const;
    static Command fromJson(const nlohmann::json& j);
};

using Response = Result<std::monostate, ApiError>;
using Cwc = CommandWithCallback<Command, Response>;

} // namespace CellSet
} // namespace Api
} // namespace DirtSim
