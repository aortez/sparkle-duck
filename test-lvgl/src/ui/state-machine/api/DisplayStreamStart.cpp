#include "DisplayStreamStart.h"
#include "core/ReflectSerializer.h"

namespace DirtSim {
namespace UiApi {
namespace DisplayStreamStart {

nlohmann::json Command::toJson() const
{
    return nlohmann::json{ { "command", "display_stream_start" }, { "fps", fps } };
}

Command Command::fromJson(const nlohmann::json& j)
{
    Command cmd;
    if (j.contains("fps")) {
        cmd.fps = j["fps"].get<int>();
    }
    return cmd;
}

nlohmann::json Okay::toJson() const
{
    return nlohmann::json{ { "started", started } };
}

} // namespace DisplayStreamStart
} // namespace UiApi
} // namespace DirtSim
