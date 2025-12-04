#pragma once

#include "ApiError.h"
#include "ApiMacros.h"
#include "core/CommandWithCallback.h"
#include "core/Result.h"
#include <cstdint>
#include <nlohmann/json.hpp>
#include <string>
#include <zpp_bits.h>

namespace DirtSim {
namespace Api {
namespace StatusGet {

DEFINE_API_NAME(StatusGet);

struct Command {
    API_COMMAND_NAME();
    nlohmann::json toJson() const;
    static Command fromJson(const nlohmann::json& j);

    // zpp_bits serialization (command has no fields).
    using serialize = zpp::bits::members<0>;
};

struct Okay {
    uint64_t timestep = 0;
    std::string scenario_id;
    uint32_t width = 0;
    uint32_t height = 0;

    API_COMMAND_NAME();
    nlohmann::json toJson() const;

    // zpp_bits serialization.
    using serialize = zpp::bits::members<4>;
};

using OkayType = Okay;
using Response = Result<OkayType, ApiError>;
using Cwc = CommandWithCallback<Command, Response>;

} // namespace StatusGet
} // namespace Api
} // namespace DirtSim
