#pragma once

#include "core/CommandWithCallback.h"
#include "core/Result.h"
#include "server/api/ApiError.h"
#include "server/api/ApiMacros.h"

#include <memory>
#include <nlohmann/json.hpp>

namespace rtc {
class WebSocket;
}

namespace DirtSim {
namespace UiApi {

namespace DisplayStreamStart {

DEFINE_API_NAME(DisplayStreamStart);

struct Command {
    int fps = 30;
    std::shared_ptr<rtc::WebSocket> ws;

    API_COMMAND_NAME();
    nlohmann::json toJson() const;
    static Command fromJson(const nlohmann::json& j);
};

struct Okay {
    bool started = true;

    API_COMMAND_NAME();
    nlohmann::json toJson() const;
};

using OkayType = Okay;
using Response = Result<OkayType, DirtSim::ApiError>;
using Cwc = CommandWithCallback<Command, Response>;

} // namespace DisplayStreamStart
} // namespace UiApi
} // namespace DirtSim
