#include "StatusGet.h"
#include "core/ReflectSerializer.h"

namespace DirtSim {
namespace Api {
namespace StatusGet {

nlohmann::json Command::toJson() const
{
    return nlohmann::json::object();
}

Command Command::fromJson(const nlohmann::json& /*j*/)
{
    return Command{};
}

nlohmann::json Okay::toJson() const
{
    return ReflectSerializer::to_json(*this);
}

} // namespace StatusGet
} // namespace Api
} // namespace DirtSim
