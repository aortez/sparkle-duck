#include "CommandProcessor.h"
#include "../World.h"
#include "../WorldInterface.h"
#include "../MaterialType.h"
#include "lvgl/src/libs/thorvg/rapidjson/stringbuffer.h"
#include "lvgl/src/libs/thorvg/rapidjson/writer.h"
#include "spdlog/spdlog.h"
#include <stdexcept>

CommandProcessor::CommandProcessor(SimulationManager* manager)
    : manager_(manager)
{
    if (!manager_) {
        throw std::runtime_error("CommandProcessor requires non-null SimulationManager");
    }
}

CommandResult CommandProcessor::processCommand(const std::string& commandJson)
{
    // Parse JSON command.
    rapidjson::Document cmd;
    cmd.Parse(commandJson.c_str());

    if (cmd.HasParseError()) {
        return CommandResult::error(CommandError(
            "JSON parse error at offset " + std::to_string(cmd.GetErrorOffset())));
    }

    if (!cmd.IsObject()) {
        return CommandResult::error(CommandError("Command must be a JSON object"));
    }

    if (!cmd.HasMember("command") || !cmd["command"].IsString()) {
        return CommandResult::error(CommandError("Command must have 'command' field with string value"));
    }

    std::string commandName = cmd["command"].GetString();
    spdlog::info("Processing command: {}", commandName);

    // Dispatch to appropriate handler.
    if (commandName == "step") {
        return handleStep(cmd);
    }
    else if (commandName == "place_material") {
        return handlePlaceMaterial(cmd);
    }
    else if (commandName == "get_state") {
        return handleGetState(cmd);
    }
    else if (commandName == "get_cell") {
        return handleGetCell(cmd);
    }
    else if (commandName == "set_gravity") {
        return handleSetGravity(cmd);
    }
    else if (commandName == "reset") {
        return handleReset(cmd);
    }
    else {
        return CommandResult::error(CommandError("Unknown command: " + commandName));
    }
}

CommandResult CommandProcessor::handleStep(const rapidjson::Document& cmd)
{
    // Parse frames parameter (default to 1).
    int frames = 1;
    if (cmd.HasMember("frames")) {
        if (!cmd["frames"].IsInt()) {
            return CommandResult::error(CommandError("'frames' must be an integer"));
        }
        frames = cmd["frames"].GetInt();
        if (frames <= 0) {
            return CommandResult::error(CommandError("'frames' must be positive"));
        }
    }

    // Step simulation.
    for (int i = 0; i < frames; ++i) {
        manager_->advanceTime(0.016); // ~60 FPS timestep.
    }

    // Return new timestep.
    rapidjson::Document response(rapidjson::kObjectType);
    auto& allocator = response.GetAllocator();
    response.AddMember("timestep", manager_->getWorld()->getTimestep(), allocator);

    return CommandResult::okay(documentToString(response));
}

CommandResult CommandProcessor::handlePlaceMaterial(const rapidjson::Document& cmd)
{
    // Validate required parameters.
    if (!cmd.HasMember("x") || !cmd["x"].IsInt()) {
        return CommandResult::error(CommandError("Missing or invalid 'x' coordinate"));
    }
    if (!cmd.HasMember("y") || !cmd["y"].IsInt()) {
        return CommandResult::error(CommandError("Missing or invalid 'y' coordinate"));
    }
    if (!cmd.HasMember("material") || !cmd["material"].IsString()) {
        return CommandResult::error(CommandError("Missing or invalid 'material' type"));
    }

    int x = cmd["x"].GetInt();
    int y = cmd["y"].GetInt();
    std::string materialName = cmd["material"].GetString();

    // Parse fill ratio (default to 1.0).
    double fill = 1.0;
    if (cmd.HasMember("fill")) {
        if (!cmd["fill"].IsNumber()) {
            return CommandResult::error(CommandError("'fill' must be a number"));
        }
        fill = cmd["fill"].GetDouble();
        if (fill < 0.0 || fill > 1.0) {
            return CommandResult::error(CommandError("'fill' must be between 0.0 and 1.0"));
        }
    }

    // Parse material type.
    MaterialType material;
    try {
        rapidjson::Document tempDoc;
        rapidjson::Value materialJson(materialName.c_str(), tempDoc.GetAllocator());
        material = materialTypeFromJson(materialJson);
    }
    catch (const std::exception& e) {
        return CommandResult::error(CommandError("Invalid material type: " + materialName));
    }

    // Validate coordinates.
    WorldInterface* worldInterface = manager_->getWorld();
    if (x < 0 || y < 0 || static_cast<uint32_t>(x) >= worldInterface->getWidth() ||
        static_cast<uint32_t>(y) >= worldInterface->getHeight()) {
        return CommandResult::error(
            CommandError("Invalid coordinates (" + std::to_string(x) + ", " + std::to_string(y) + ")"));
    }

    // Place material.
    worldInterface->addMaterialAtCell(x, y, material, fill);

    // Return empty success.
    return CommandResult::okay("{}");
}

CommandResult CommandProcessor::handleGetState(const rapidjson::Document& /*cmd*/)
{
    // Cast to World* (safe since we only have one world type now).
    World* world = dynamic_cast<World*>(manager_->getWorld());
    if (!world) {
        return CommandResult::error(CommandError("World type mismatch"));
    }

    // Serialize complete world state.
    rapidjson::Document worldJson = world->toJSON();
    return CommandResult::okay(documentToString(worldJson));
}

CommandResult CommandProcessor::handleGetCell(const rapidjson::Document& cmd)
{
    // Validate parameters.
    if (!cmd.HasMember("x") || !cmd["x"].IsInt()) {
        return CommandResult::error(CommandError("Missing or invalid 'x' coordinate"));
    }
    if (!cmd.HasMember("y") || !cmd["y"].IsInt()) {
        return CommandResult::error(CommandError("Missing or invalid 'y' coordinate"));
    }

    int x = cmd["x"].GetInt();
    int y = cmd["y"].GetInt();

    // Cast to World* (safe since we only have one world type now).
    World* world = dynamic_cast<World*>(manager_->getWorld());
    if (!world) {
        return CommandResult::error(CommandError("World type mismatch"));
    }

    // Validate coordinates.
    if (x < 0 || y < 0 || static_cast<uint32_t>(x) >= world->getWidth() ||
        static_cast<uint32_t>(y) >= world->getHeight()) {
        return CommandResult::error(
            CommandError("Invalid coordinates (" + std::to_string(x) + ", " + std::to_string(y) + ")"));
    }

    // Get cell and serialize.
    rapidjson::Document response(rapidjson::kObjectType);
    auto& allocator = response.GetAllocator();

    const Cell& cell = world->at(x, y);
    response.CopyFrom(cell.toJson(allocator), allocator);

    return CommandResult::okay(documentToString(response));
}

CommandResult CommandProcessor::handleSetGravity(const rapidjson::Document& cmd)
{
    // Validate parameter.
    if (!cmd.HasMember("value") || !cmd["value"].IsNumber()) {
        return CommandResult::error(CommandError("Missing or invalid 'value' parameter"));
    }

    double gravity = cmd["value"].GetDouble();

    // Set gravity.
    manager_->getWorld()->setGravity(gravity);
    spdlog::info("Gravity set to {}", gravity);

    // Return empty success.
    return CommandResult::okay("{}");
}

CommandResult CommandProcessor::handleReset(const rapidjson::Document& /*cmd*/)
{
    manager_->reset();
    spdlog::info("World reset");

    // Return empty success.
    return CommandResult::okay("{}");
}

std::string CommandProcessor::documentToString(const rapidjson::Document& doc) const
{
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    doc.Accept(writer);
    return buffer.GetString();
}
