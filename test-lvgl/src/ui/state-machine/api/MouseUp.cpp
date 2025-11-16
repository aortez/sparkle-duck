#include "MouseUp.h"

namespace DirtSim {
namespace UiApi {
namespace MouseUp {

nlohmann::json Command::toJson() const
{
    return nlohmann::json{ { "pixelX", pixelX }, { "pixelY", pixelY } };
}

Command Command::fromJson(const nlohmann::json& j)
{
    return Command{ j["pixelX"].get<int>(), j["pixelY"].get<int>() };
}

} // namespace MouseUp
} // namespace UiApi
} // namespace DirtSim
