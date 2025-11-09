#include "SimRun.h"

namespace DirtSim {
namespace UiApi {
namespace SimRun {

nlohmann::json Command::toJson() const
{
    return nlohmann::json{ { "command", "sim_run" } };
}

Command Command::fromJson(const nlohmann::json& /*j*/)
{
    // SimRun command has no parameters for UI.
    return Command{};
}

nlohmann::json Okay::toJson() const
{
    return nlohmann::json{ { "running", running } };
}

} // namespace SimRun
} // namespace UiApi
} // namespace DirtSim
