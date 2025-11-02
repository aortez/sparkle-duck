#include "StepN.h"
#include "../../core/ReflectSerializer.h"

namespace DirtSim {
namespace Api {
namespace StepN {

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

} // namespace StepN
} // namespace Api
} // namespace DirtSim
