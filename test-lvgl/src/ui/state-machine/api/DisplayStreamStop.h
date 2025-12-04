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

namespace DisplayStreamStop {

DEFINE_API_NAME(DisplayStreamStop);

struct Command {
    std::shared_ptr<rtc::WebSocket> ws;

    API_COMMAND_NAME();
    nlohmann::json toJson() const;
    static Command fromJson(const nlohmann::json& j);
};

struct Okay {
    bool stopped = true;

    API_COMMAND_NAME();
    nlohmann::json toJson() const;
};

using OkayType = Okay;
using Response = Result<OkayType, DirtSim::ApiError>;
using Cwc = CommandWithCallback<Command, Response>;

} // namespace DisplayStreamStop
} // namespace UiApi
} // namespace DirtSim
