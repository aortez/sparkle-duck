#include "ScreenGrab.h"

namespace DirtSim {
namespace UiApi {
namespace ScreenGrab {

nlohmann::json Command::toJson() const
{
    return nlohmann::json{};
}

Command Command::fromJson(const nlohmann::json& /*j*/)
{
    return Command{};
}

nlohmann::json Okay::toJson() const
{
    return nlohmann::json{ { "data", data } };
}

} // namespace ScreenGrab
} // namespace UiApi
} // namespace DirtSim
