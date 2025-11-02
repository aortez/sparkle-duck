#pragma once

#include "../../../core/CommandWithCallback.h"
#include "../../../core/Result.h"
#include "../../../server/api/ApiError.h"
#include <nlohmann/json.hpp>

namespace DirtSim {
namespace UiApi {

namespace SimRun {

struct Command {
    nlohmann::json toJson() const;
    static Command fromJson(const nlohmann::json& j);
};

struct Okay {
    bool running;

    nlohmann::json toJson() const;
};

using Response = Result<Okay, ApiError>;
using Cwc = CommandWithCallback<Command, Response>;

} // namespace SimRun
} // namespace UiApi
} // namespace DirtSim
