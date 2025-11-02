#pragma once

#include "../../../core/CommandWithCallback.h"
#include "../../../core/Result.h"
#include "../../../server/api/ApiError.h"
#include <nlohmann/json.hpp>
#include <string>
#include <variant>

namespace DirtSim {
namespace UiApi {

namespace Screenshot {

struct Command {
    std::string filepath; // Optional: if empty, use default name.

    nlohmann::json toJson() const;
    static Command fromJson(const nlohmann::json& j);
};

struct Okay {
    std::string filepath; // Actual path where screenshot was saved.

    nlohmann::json toJson() const;
};

using Response = Result<Okay, ApiError>;
using Cwc = CommandWithCallback<Command, Response>;

} // namespace Screenshot
} // namespace UiApi
} // namespace DirtSim
