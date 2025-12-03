#include "ScreenGrab.h"

namespace DirtSim {
namespace UiApi {
namespace ScreenGrab {

nlohmann::json Command::toJson() const
{
    nlohmann::json j;
    if (scale != 1.0) {
        j["scale"] = scale;
    }
    return j;
}

Command Command::fromJson(const nlohmann::json& j)
{
    Command cmd;
    if (j.contains("scale")) {
        cmd.scale = j["scale"].get<double>();
    }
    return cmd;
}

nlohmann::json Okay::toJson() const
{
    return nlohmann::json{ { "pixels", pixels }, { "width", width }, { "height", height } };
}

} // namespace ScreenGrab
} // namespace UiApi
} // namespace DirtSim
