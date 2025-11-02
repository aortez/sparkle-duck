#pragma once

#include "../../core/CommandWithCallback.h"
#include "../../core/Result.h"
#include "ApiError.h"
#include <nlohmann/json.hpp>
#include <variant>

namespace DirtSim {
namespace Api {

namespace Reset {

struct Command {
    nlohmann::json toJson() const;
    static Command fromJson(const nlohmann::json& j);
};

using Response = Result<std::monostate, ApiError>;
using Cwc = CommandWithCallback<Command, Response>;

} // namespace Reset
} // namespace Api
} // namespace DirtSim
