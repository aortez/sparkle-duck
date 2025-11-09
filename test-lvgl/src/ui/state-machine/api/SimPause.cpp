#include "SimPause.h"

namespace DirtSim {
namespace UiApi {
namespace SimPause {

nlohmann::json Command::toJson() const
{
    return nlohmann::json{ { "command", "sim_pause" } };
}

Command Command::fromJson(const nlohmann::json& /*j*/)
{
    // SimPause command has no parameters.
    return Command{};
}

nlohmann::json Okay::toJson() const
{
    return nlohmann::json{ { "paused", paused } };
}

} // namespace SimPause
} // namespace UiApi
} // namespace DirtSim
