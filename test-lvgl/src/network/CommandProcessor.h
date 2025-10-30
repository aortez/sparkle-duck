#pragma once

#include "CommandResult.h"
#include "../SimulationManager.h"
#include "lvgl/src/libs/thorvg/rapidjson/document.h"
#include <string>

/**
 * CommandProcessor handles JSON commands for remote simulation control.
 *
 * Parses JSON command strings, executes them on the simulation, and returns
 * Result-wrapped responses. All responses follow the pattern:
 *   Success: {"value": {...data...}}
 *   Error:   {"error": "error message"}
 */
class CommandProcessor {
public:
    /**
     * Construct a command processor for a simulation.
     * @param manager The simulation manager to control (non-owning pointer).
     */
    explicit CommandProcessor(SimulationManager* manager);

    /**
     * Process a JSON command and return a Result-wrapped response.
     * @param commandJson The JSON command string.
     * @return CommandResult containing response JSON string or error message.
     */
    CommandResult processCommand(const std::string& commandJson);

private:
    SimulationManager* manager_; // Non-owning pointer to simulation.

    // Command handlers - each returns a CommandResult.
    CommandResult handleStep(const rapidjson::Document& cmd);
    CommandResult handlePlaceMaterial(const rapidjson::Document& cmd);
    CommandResult handleGetState(const rapidjson::Document& cmd);
    CommandResult handleGetCell(const rapidjson::Document& cmd);
    CommandResult handleSetGravity(const rapidjson::Document& cmd);
    CommandResult handleReset(const rapidjson::Document& cmd);

    // Helper to convert rapidjson::Document to JSON string.
    std::string documentToString(const rapidjson::Document& doc) const;
};
