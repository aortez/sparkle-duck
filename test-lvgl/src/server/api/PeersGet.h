#pragma once

#include "ApiError.h"
#include "ApiMacros.h"
#include "core/CommandWithCallback.h"
#include "core/Result.h"
#include "server/network/PeerDiscovery.h"

#include <nlohmann/json.hpp>
#include <vector>

namespace DirtSim {
namespace Api {

namespace PeersGet {

DEFINE_API_NAME(PeersGet);

struct Command {
    API_COMMAND_NAME();
    nlohmann::json toJson() const;
    static Command fromJson(const nlohmann::json& j);
};

struct Okay {
    std::vector<Server::PeerInfo> peers;

    API_COMMAND_NAME();
    nlohmann::json toJson() const;
};

using Response = Result<Okay, ApiError>;
using Cwc = CommandWithCallback<Command, Response>;

} // namespace PeersGet
} // namespace Api
} // namespace DirtSim
