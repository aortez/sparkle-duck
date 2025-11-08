#include "StateGet.h"
#include "core/ReflectSerializer.h"

namespace DirtSim {
namespace Api {
namespace StateGet {

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
    // WorldData uses automatic serialization via ADL.
    return ReflectSerializer::to_json(worldData);
}

} // namespace StateGet
} // namespace Api
} // namespace DirtSim
