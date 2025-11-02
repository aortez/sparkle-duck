#include "CommandDeserializerJson.h"
#include "../api/Exit.h"
#include "../api/MouseDown.h"
#include "../api/MouseMove.h"
#include "../api/MouseUp.h"
#include "../api/Screenshot.h"
#include "../api/SimPause.h"
#include "../api/SimRun.h"
#include <spdlog/spdlog.h>

namespace DirtSim {
namespace Ui {

Result<UiApiCommand, ApiError> CommandDeserializerJson::deserialize(const std::string& commandJson)
{
    // Parse JSON command.
    nlohmann::json cmd;

    try {
        cmd = nlohmann::json::parse(commandJson);
    } catch (const nlohmann::json::parse_error& e) {
        return Result<UiApiCommand, ApiError>::error(
            ApiError(std::string("JSON parse error: ") + e.what()));
    }

    if (!cmd.is_object()) {
        return Result<UiApiCommand, ApiError>::error(ApiError("Command must be a JSON object"));
    }

    if (!cmd.contains("command") || !cmd["command"].is_string()) {
        return Result<UiApiCommand, ApiError>::error(
            ApiError("Command must have 'command' field with string value"));
    }

    std::string commandName = cmd["command"].get<std::string>();
    spdlog::debug("UI: Deserializing command: {}", commandName);

    // Dispatch to appropriate handler.
    try {
        if (commandName == "exit") {
            return Result<UiApiCommand, ApiError>::okay(UiApi::Exit::Command::fromJson(cmd));
        }
        else if (commandName == "sim_run") {
            return Result<UiApiCommand, ApiError>::okay(UiApi::SimRun::Command::fromJson(cmd));
        }
        else if (commandName == "sim_pause") {
            return Result<UiApiCommand, ApiError>::okay(UiApi::SimPause::Command::fromJson(cmd));
        }
        else if (commandName == "screenshot") {
            return Result<UiApiCommand, ApiError>::okay(UiApi::Screenshot::Command::fromJson(cmd));
        }
        else if (commandName == "mouse_down") {
            return Result<UiApiCommand, ApiError>::okay(UiApi::MouseDown::Command::fromJson(cmd));
        }
        else if (commandName == "mouse_move") {
            return Result<UiApiCommand, ApiError>::okay(UiApi::MouseMove::Command::fromJson(cmd));
        }
        else if (commandName == "mouse_up") {
            return Result<UiApiCommand, ApiError>::okay(UiApi::MouseUp::Command::fromJson(cmd));
        }
        else {
            return Result<UiApiCommand, ApiError>::error(
                ApiError("Unknown UI command: " + commandName));
        }
    }
    catch (const nlohmann::json::exception& e) {
        return Result<UiApiCommand, ApiError>::error(
            ApiError(std::string("JSON deserialization error: ") + e.what()));
    }
}

} // namespace Ui
} // namespace DirtSim
