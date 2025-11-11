#include "PhysicsSettingsSet.h"
#include "core/ReflectSerializer.h"

namespace DirtSim {
namespace Api {
namespace PhysicsSettingsSet {

nlohmann::json Command::toJson() const
{
    return ReflectSerializer::to_json(*this);
}

Command Command::fromJson(const nlohmann::json& j)
{
    return ReflectSerializer::from_json<Command>(j);
}

} // namespace PhysicsSettingsSet
} // namespace Api
} // namespace DirtSim
