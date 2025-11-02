#include "CommandDeserializerJson.h"
#include "../../core/MaterialType.h"
#include <spdlog/spdlog.h>

namespace DirtSim {
namespace Server {

Result<ApiCommand, ApiError> CommandDeserializerJson::deserialize(const std::string& commandJson)
{
    // Parse JSON command.
    rapidjson::Document cmd;
    cmd.Parse(commandJson.c_str());

    if (cmd.HasParseError()) {
        return Result<ApiCommand, ApiError>::error(
            ApiError("JSON parse error at offset " + std::to_string(cmd.GetErrorOffset())));
    }

    if (!cmd.IsObject()) {
        return Result<ApiCommand, ApiError>::error(ApiError("Command must be a JSON object"));
    }

    if (!cmd.HasMember("command") || !cmd["command"].IsString()) {
        return Result<ApiCommand, ApiError>::error(
            ApiError("Command must have 'command' field with string value"));
    }

    std::string commandName = cmd["command"].GetString();
    spdlog::debug("Deserializing command: {}", commandName);

    // Dispatch to appropriate handler.
    if (commandName == "step") {
        return handleStepN(cmd);
    }
    else if (commandName == "place_material") {
        return handleCellSet(cmd);
    }
    else if (commandName == "get_state") {
        return handleStateGet(cmd);
    }
    else if (commandName == "get_cell") {
        return handleCellGet(cmd);
    }
    else if (commandName == "set_gravity") {
        return handleGravitySet(cmd);
    }
    else if (commandName == "reset") {
        return handleReset(cmd);
    }
    else {
        return Result<ApiCommand, ApiError>::error(ApiError("Unknown command: " + commandName));
    }
}

Result<ApiCommand, ApiError> CommandDeserializerJson::handleStepN(const rapidjson::Document& cmd)
{
    // Parse frames parameter (default to 1).
    int frames = 1;
    if (cmd.HasMember("frames")) {
        if (!cmd["frames"].IsInt()) {
            return Result<ApiCommand, ApiError>::error(ApiError("'frames' must be an integer"));
        }
        frames = cmd["frames"].GetInt();
    }

    Api::StepN::Command command;
    command.frames = frames;
    return Result<ApiCommand, ApiError>::okay(command);
}

Result<ApiCommand, ApiError> CommandDeserializerJson::handleCellSet(const rapidjson::Document& cmd)
{
    // Validate required parameters.
    if (!cmd.HasMember("x") || !cmd["x"].IsInt()) {
        return Result<ApiCommand, ApiError>::error(ApiError("Missing or invalid 'x' coordinate"));
    }
    if (!cmd.HasMember("y") || !cmd["y"].IsInt()) {
        return Result<ApiCommand, ApiError>::error(ApiError("Missing or invalid 'y' coordinate"));
    }
    if (!cmd.HasMember("material") || !cmd["material"].IsString()) {
        return Result<ApiCommand, ApiError>::error(ApiError("Missing or invalid 'material' type"));
    }

    int x = cmd["x"].GetInt();
    int y = cmd["y"].GetInt();
    std::string materialName = cmd["material"].GetString();

    // Parse fill ratio (default to 1.0).
    double fill = 1.0;
    if (cmd.HasMember("fill")) {
        if (!cmd["fill"].IsNumber()) {
            return Result<ApiCommand, ApiError>::error(ApiError("'fill' must be a number"));
        }
        fill = cmd["fill"].GetDouble();
    }

    // Parse material type.
    MaterialType material;
    try {
        rapidjson::Document tempDoc;
        rapidjson::Value materialJson(materialName.c_str(), tempDoc.GetAllocator());
        material = materialTypeFromJson(materialJson);
    }
    catch (const std::exception& e) {
        return Result<ApiCommand, ApiError>::error(ApiError("Invalid material type: " + materialName));
    }

    Api::CellSet::Command command;
    command.x = x;
    command.y = y;
    command.material = material;
    command.fill = fill;
    return Result<ApiCommand, ApiError>::okay(command);
}

Result<ApiCommand, ApiError> CommandDeserializerJson::handleStateGet(const rapidjson::Document& /*cmd*/)
{
    // No parameters needed.
    Api::StateGet::Command command;
    return Result<ApiCommand, ApiError>::okay(command);
}

Result<ApiCommand, ApiError> CommandDeserializerJson::handleCellGet(const rapidjson::Document& cmd)
{
    // Validate parameters.
    if (!cmd.HasMember("x") || !cmd["x"].IsInt()) {
        return Result<ApiCommand, ApiError>::error(ApiError("Missing or invalid 'x' coordinate"));
    }
    if (!cmd.HasMember("y") || !cmd["y"].IsInt()) {
        return Result<ApiCommand, ApiError>::error(ApiError("Missing or invalid 'y' coordinate"));
    }

    int x = cmd["x"].GetInt();
    int y = cmd["y"].GetInt();

    Api::CellGet::Command command;
    command.x = x;
    command.y = y;
    return Result<ApiCommand, ApiError>::okay(command);
}

Result<ApiCommand, ApiError> CommandDeserializerJson::handleGravitySet(const rapidjson::Document& cmd)
{
    // Validate parameter.
    if (!cmd.HasMember("value") || !cmd["value"].IsNumber()) {
        return Result<ApiCommand, ApiError>::error(ApiError("Missing or invalid 'value' parameter"));
    }

    double gravity = cmd["value"].GetDouble();

    Api::GravitySet::Command command;
    command.gravity = gravity;
    return Result<ApiCommand, ApiError>::okay(command);
}

Result<ApiCommand, ApiError> CommandDeserializerJson::handleReset(const rapidjson::Document& /*cmd*/)
{
    // No parameters needed.
    Api::Reset::Command command;
    return Result<ApiCommand, ApiError>::okay(command);
}

} // namespace Server
} // namespace DirtSim
