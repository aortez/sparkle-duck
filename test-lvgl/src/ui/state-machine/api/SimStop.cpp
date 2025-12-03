#include "SimStop.h"

namespace DirtSim {
namespace UiApi {
namespace SimStop {

nlohmann::json Command::toJson() const
{
    return nlohmann::json{ { "command", "sim_stop" } };
}

Command Command::fromJson(const nlohmann::json& /*j*/)
{
    // SimStop command has no parameters.
    return Command{};
}

nlohmann::json Okay::toJson() const
{
    return nlohmann::json{ { "stopped", stopped } };
}

} // namespace SimStop
} // namespace UiApi
} // namespace DirtSim
