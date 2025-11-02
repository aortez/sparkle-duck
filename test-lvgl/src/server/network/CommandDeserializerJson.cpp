#include "CommandDeserializerJson.h"
#include "../api/CellGet.h"
#include "../api/CellSet.h"
#include "../api/GravitySet.h"
#include "../api/Reset.h"
#include "../api/StateGet.h"
#include "../api/StepN.h"
#include <spdlog/spdlog.h>

namespace DirtSim {
namespace Server {

Result<ApiCommand, ApiError> CommandDeserializerJson::deserialize(const std::string& commandJson)
{
    // Parse JSON command.
    nlohmann::json cmd;

    try {
        cmd = nlohmann::json::parse(commandJson);
    } catch (const nlohmann::json::parse_error& e) {
        return Result<ApiCommand, ApiError>::error(
            ApiError(std::string("JSON parse error: ") + e.what()));
    }

    if (!cmd.is_object()) {
        return Result<ApiCommand, ApiError>::error(ApiError("Command must be a JSON object"));
    }

    if (!cmd.contains("command") || !cmd["command"].is_string()) {
        return Result<ApiCommand, ApiError>::error(
            ApiError("Command must have 'command' field with string value"));
    }

    std::string commandName = cmd["command"].get<std::string>();
    spdlog::debug("Deserializing command: {}", commandName);

    // Dispatch to appropriate handler.
    try {
        if (commandName == "step") {
            return Result<ApiCommand, ApiError>::okay(Api::StepN::Command::fromJson(cmd));
        }
        else if (commandName == "place_material") {
            return Result<ApiCommand, ApiError>::okay(Api::CellSet::Command::fromJson(cmd));
        }
        else if (commandName == "get_state") {
            return Result<ApiCommand, ApiError>::okay(Api::StateGet::Command::fromJson(cmd));
        }
        else if (commandName == "get_cell") {
            return Result<ApiCommand, ApiError>::okay(Api::CellGet::Command::fromJson(cmd));
        }
        else if (commandName == "set_gravity") {
            return Result<ApiCommand, ApiError>::okay(Api::GravitySet::Command::fromJson(cmd));
        }
        else if (commandName == "reset") {
            return Result<ApiCommand, ApiError>::okay(Api::Reset::Command::fromJson(cmd));
        }
        else {
            return Result<ApiCommand, ApiError>::error(ApiError("Unknown command: " + commandName));
        }
    } catch (const std::exception& e) {
        return Result<ApiCommand, ApiError>::error(
            ApiError(std::string("Error deserializing command: ") + e.what()));
    }
}

} // namespace Server
} // namespace DirtSim
