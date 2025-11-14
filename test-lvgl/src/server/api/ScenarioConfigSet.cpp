#include "ScenarioConfigSet.h"

namespace DirtSim {
namespace Api {
namespace ScenarioConfigSet {

nlohmann::json Command::toJson() const
{
    nlohmann::json j;
    j["config"] = config; // Uses ADL to_json from ScenarioConfig.h.
    return j;
}

Command Command::fromJson(const nlohmann::json& j)
{
    Command cmd;
    if (j.contains("config")) {
        cmd.config = j["config"].get<ScenarioConfig>(); // Uses ADL from_json from ScenarioConfig.h.
    }
    return cmd;
}

nlohmann::json Okay::toJson() const
{
    nlohmann::json j;
    j["success"] = success;
    return j;
}

} // namespace ScenarioConfigSet
} // namespace Api
} // namespace DirtSim
