#pragma once

#include "ApiError.h"
#include "core/CommandWithCallback.h"
#include "core/Result.h"
#include <nlohmann/json.hpp>
#include <string>

namespace DirtSim {
namespace Api {

namespace DiagramGet {

struct Command {
    nlohmann::json toJson() const;
    static Command fromJson(const nlohmann::json& j);
};

struct Okay {
    std::string diagram;

    static constexpr const char* name() { return "diagram_get"; }
    nlohmann::json toJson() const;
};

using Response = Result<Okay, ApiError>;
using Cwc = CommandWithCallback<Command, Response>;

} // namespace DiagramGet
} // namespace Api
} // namespace DirtSim
