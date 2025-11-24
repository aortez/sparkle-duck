#include "CommandDeserializerJson.h"
#include "ui/state-machine/api/DrawDebugToggle.h"
#include "ui/state-machine/api/Exit.h"
#include "ui/state-machine/api/MouseDown.h"
#include "ui/state-machine/api/MouseMove.h"
#include "ui/state-machine/api/MouseUp.h"
#include "ui/state-machine/api/RenderModeSelect.h"
#include "ui/state-machine/api/Screenshot.h"
#include "ui/state-machine/api/SimPause.h"
#include "ui/state-machine/api/SimRun.h"
#include "ui/state-machine/api/StatusGet.h"
#include <cctype>
#include <spdlog/spdlog.h>

namespace DirtSim {
namespace Ui {

namespace {
/**
 * @brief Convert PascalCase to snake_case for command names.
 *
 * This allows the C++ API to use PascalCase (e.g., "SimRun") and the
 * WebSocket API to use snake_case (e.g., "sim_run").
 */
std::string toSnakeCase(const std::string& str)
{
    std::string result;
    for (char c : str) {
        if (std::isupper(c) && !result.empty()) {
            result += '_';
        }
        result += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return result;
}
} // namespace

Result<UiApiCommand, ApiError> CommandDeserializerJson::deserialize(const std::string& commandJson)
{
    // Parse JSON command.
    nlohmann::json cmd;

    try {
        cmd = nlohmann::json::parse(commandJson);
    }
    catch (const nlohmann::json::parse_error& e) {
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

    std::string commandName = toSnakeCase(cmd["command"].get<std::string>());
    spdlog::debug("UI: Deserializing command: {}", commandName);

    // Dispatch to appropriate handler.
    try {
        if (commandName == "draw_debug_toggle") {
            return Result<UiApiCommand, ApiError>::okay(
                UiApi::DrawDebugToggle::Command::fromJson(cmd));
        }
        else if (commandName == "exit") {
            return Result<UiApiCommand, ApiError>::okay(UiApi::Exit::Command::fromJson(cmd));
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
        else if (commandName == "render_mode_select") {
            return Result<UiApiCommand, ApiError>::okay(
                UiApi::RenderModeSelect::Command::fromJson(cmd));
        }
        else if (commandName == "screenshot") {
            return Result<UiApiCommand, ApiError>::okay(UiApi::Screenshot::Command::fromJson(cmd));
        }
        else if (commandName == "sim_pause") {
            return Result<UiApiCommand, ApiError>::okay(UiApi::SimPause::Command::fromJson(cmd));
        }
        else if (commandName == "sim_run") {
            return Result<UiApiCommand, ApiError>::okay(UiApi::SimRun::Command::fromJson(cmd));
        }
        else if (commandName == "status_get") {
            return Result<UiApiCommand, ApiError>::okay(UiApi::StatusGet::Command::fromJson(cmd));
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
