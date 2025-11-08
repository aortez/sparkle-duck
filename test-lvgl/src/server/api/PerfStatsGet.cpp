#include "PerfStatsGet.h"
#include "core/ReflectSerializer.h"

namespace DirtSim {
namespace Api {
namespace PerfStatsGet {

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
    return ReflectSerializer::to_json(*this);
}

} // namespace PerfStatsGet
} // namespace Api
} // namespace DirtSim
