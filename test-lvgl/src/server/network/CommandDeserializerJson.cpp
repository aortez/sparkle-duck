#include "CommandDeserializerJson.h"
#include "server/api/CellGet.h"
#include "server/api/CellSet.h"
#include "server/api/DiagramGet.h"
#include "server/api/Exit.h"
#include "server/api/GravitySet.h"
#include "server/api/Reset.h"
#include "server/api/ScenarioConfigSet.h"
#include "server/api/SimRun.h"
#include "server/api/StateGet.h"
#include "server/api/StepN.h"
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
        else if (commandName == "cell_get") {
            return Result<ApiCommand, ApiError>::okay(Api::CellGet::Command::fromJson(cmd));
        }
        else if (commandName == "cell_set") {
            return Result<ApiCommand, ApiError>::okay(Api::CellSet::Command::fromJson(cmd));
        }
        else if (commandName == "diagram_get") {
            return Result<ApiCommand, ApiError>::okay(Api::DiagramGet::Command::fromJson(cmd));
        }
        else if (commandName == "exit") {
            return Result<ApiCommand, ApiError>::okay(Api::Exit::Command::fromJson(cmd));
        }
        else if (commandName == "gravity_set") {
            return Result<ApiCommand, ApiError>::okay(Api::GravitySet::Command::fromJson(cmd));
        }
        else if (commandName == "reset") {
            return Result<ApiCommand, ApiError>::okay(Api::Reset::Command::fromJson(cmd));
        }
        else if (commandName == "scenario_config_set") {
            return Result<ApiCommand, ApiError>::okay(Api::ScenarioConfigSet::Command::fromJson(cmd));
        }
        else if (commandName == "sim_run") {
            return Result<ApiCommand, ApiError>::okay(Api::SimRun::Command::fromJson(cmd));
        }
        else if (commandName == "state_get") {
            return Result<ApiCommand, ApiError>::okay(Api::StateGet::Command::fromJson(cmd));
        }
        // Legacy aliases for backward compatibility.
        else if (commandName == "place_material") {
            return Result<ApiCommand, ApiError>::okay(Api::CellSet::Command::fromJson(cmd));
        }
        else if (commandName == "get_cell") {
            return Result<ApiCommand, ApiError>::okay(Api::CellGet::Command::fromJson(cmd));
        }
        else if (commandName == "get_state") {
            return Result<ApiCommand, ApiError>::okay(Api::StateGet::Command::fromJson(cmd));
        }
        else if (commandName == "set_gravity") {
            return Result<ApiCommand, ApiError>::okay(Api::GravitySet::Command::fromJson(cmd));
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
