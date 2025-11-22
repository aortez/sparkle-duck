#pragma once

#include "core/CommandWithCallback.h"
#include "core/Result.h"
#include "server/api/ApiError.h"
#include "server/api/ApiMacros.h"
#include <nlohmann/json.hpp>
#include <string>

namespace DirtSim {
namespace UiApi {

namespace StatusGet {

DEFINE_API_NAME(StatusGet);

struct Command {
    API_COMMAND_NAME();
    nlohmann::json toJson() const;
    static Command fromJson(const nlohmann::json& j);
};

struct Okay {
    std::string state; // UI state machine current state.
    bool connected_to_server = false;
    std::string server_url;
    uint32_t display_width = 0;
    uint32_t display_height = 0;
    double fps = 0.0;

    API_COMMAND_NAME();
    nlohmann::json toJson() const;
    static Okay fromJson(const nlohmann::json& j);
};

using Response = Result<Okay, ApiError>;
using Cwc = CommandWithCallback<Command, Response>;

} // namespace StatusGet
} // namespace UiApi
} // namespace DirtSim
