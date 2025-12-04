#pragma once

#include "ApiError.h"
#include "ApiMacros.h"
#include "core/CommandWithCallback.h"
#include "core/Result.h"
#include <cstdint>
#include <nlohmann/json.hpp>
#include <string>

namespace DirtSim {
namespace Api {
namespace StatusGet {

DEFINE_API_NAME(StatusGet);

struct Command {
    API_COMMAND_NAME();
    nlohmann::json toJson() const;
    static Command fromJson(const nlohmann::json& j);
};

struct Okay {
    uint64_t timestep = 0;
    std::string scenario_id;
    uint32_t width = 0;
    uint32_t height = 0;

    API_COMMAND_NAME();
    nlohmann::json toJson() const;
};

using OkayType = Okay;
using Response = Result<OkayType, ApiError>;
using Cwc = CommandWithCallback<Command, Response>;

} // namespace StatusGet
} // namespace Api
} // namespace DirtSim
