#include "MouseDown.h"

namespace DirtSim {
namespace UiApi {
namespace MouseDown {

nlohmann::json Command::toJson() const
{
    return nlohmann::json{ { "pixelX", pixelX }, { "pixelY", pixelY } };
}

Command Command::fromJson(const nlohmann::json& j)
{
    return Command{ j["pixelX"].get<int>(), j["pixelY"].get<int>() };
}

} // namespace MouseDown
} // namespace UiApi
} // namespace DirtSim
