#include "DisplayStreamStop.h"

namespace DirtSim {
namespace UiApi {
namespace DisplayStreamStop {

nlohmann::json Command::toJson() const
{
    return nlohmann::json{ { "command", "display_stream_stop" } };
}

Command Command::fromJson(const nlohmann::json&)
{
    return Command{};
}

nlohmann::json Okay::toJson() const
{
    return nlohmann::json{ { "stopped", stopped } };
}

} // namespace DisplayStreamStop
} // namespace UiApi
} // namespace DirtSim
