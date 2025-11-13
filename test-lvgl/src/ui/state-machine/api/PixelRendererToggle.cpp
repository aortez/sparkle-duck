#include "PixelRendererToggle.h"
#include "core/ReflectSerializer.h"

namespace DirtSim {
namespace UiApi {
namespace PixelRendererToggle {

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

Okay Okay::fromJson(const nlohmann::json& j)
{
    return ReflectSerializer::from_json<Okay>(j);
}

} // namespace PixelRendererToggle
} // namespace UiApi
} // namespace DirtSim
