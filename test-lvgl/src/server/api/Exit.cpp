#include "Exit.h"
#include "core/ReflectSerializer.h"

namespace DirtSim {
namespace Api {
namespace Exit {

nlohmann::json Command::toJson() const
{
    return ReflectSerializer::to_json(*this);
}

Command Command::fromJson(const nlohmann::json& j)
{
    return ReflectSerializer::from_json<Command>(j);
}

} // namespace Exit
} // namespace Api
} // namespace DirtSim
