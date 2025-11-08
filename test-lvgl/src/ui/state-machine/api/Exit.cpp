#include "Exit.h"

namespace DirtSim {
namespace UiApi {
namespace Exit {

nlohmann::json Command::toJson() const
{
    return nlohmann::json{{"command", "exit"}};
}

Command Command::fromJson(const nlohmann::json& /*j*/)
{
    // Exit command has no parameters.
    return Command{};
}

} // namespace Exit
} // namespace UiApi
} // namespace DirtSim
