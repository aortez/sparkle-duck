#include "Screenshot.h"

namespace DirtSim {
namespace UiApi {
namespace Screenshot {

nlohmann::json Command::toJson() const
{
    nlohmann::json j{{"command", "screenshot"}};
    if (!filepath.empty()) {
        j["filepath"] = filepath;
    }
    return j;
}

Command Command::fromJson(const nlohmann::json& j)
{
    Command cmd;
    if (j.contains("filepath")) {
        cmd.filepath = j["filepath"].get<std::string>();
    }
    return cmd;
}

nlohmann::json Okay::toJson() const
{
    return nlohmann::json{{"filepath", filepath}};
}

} // namespace Screenshot
} // namespace UiApi
} // namespace DirtSim
