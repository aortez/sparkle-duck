#include "TimerStatsGet.h"
#include "core/ReflectSerializer.h"

namespace DirtSim {
namespace Api {
namespace TimerStatsGet {

nlohmann::json Command::toJson() const
{
    return ReflectSerializer::to_json(*this);
}

Command Command::fromJson(const nlohmann::json& j)
{
    return ReflectSerializer::from_json<Command>(j);
}

nlohmann::json Okay::toJson() const
{
    nlohmann::json j = nlohmann::json::object();

    for (const auto& [name, entry] : timers) {
        j[name] = ReflectSerializer::to_json(entry);
    }

    return j;
}

} // namespace TimerStatsGet
} // namespace Api
} // namespace DirtSim
