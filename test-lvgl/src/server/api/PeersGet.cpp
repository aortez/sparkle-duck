#include "PeersGet.h"

namespace DirtSim {
namespace Api {
namespace PeersGet {

nlohmann::json Command::toJson() const
{
    return nlohmann::json{ { "command", "peers_get" } };
}

Command Command::fromJson(const nlohmann::json&)
{
    return Command{};
}

namespace {
std::string roleToString(Server::PeerRole role)
{
    switch (role) {
        case Server::PeerRole::Physics:
            return "physics";
        case Server::PeerRole::Ui:
            return "ui";
        default:
            return "unknown";
    }
}
} // namespace

nlohmann::json Okay::toJson() const
{
    nlohmann::json result;
    result["peers"] = nlohmann::json::array();

    for (const auto& peer : peers) {
        result["peers"].push_back({ { "name", peer.name },
                                    { "host", peer.host },
                                    { "address", peer.address },
                                    { "port", peer.port },
                                    { "role", roleToString(peer.role) } });
    }

    return result;
}

} // namespace PeersGet
} // namespace Api
} // namespace DirtSim
