#pragma once

#include "../api/ApiCommand.h"
#include "lvgl/src/libs/thorvg/rapidjson/document.h"
#include <string>

namespace DirtSim {
namespace Server {

/**
 * @brief Deserializes JSON command strings into API command structs.
 *
 * Pure deserialization - converts JSON to Command objects without
 * any side effects. Does not know about state machines, callbacks,
 * or network layers.
 */
class CommandDeserializerJson {
public:
    /**
     * @brief Deserialize JSON command string into Command variant.
     * @param commandJson The JSON command string.
     * @return Result containing Command or error message.
     */
    Result<ApiCommand, ApiError> deserialize(const std::string& commandJson);

private:
    // Command deserializers - each creates a Command struct from JSON.
    Result<ApiCommand, ApiError> handleStepN(const rapidjson::Document& cmd);
    Result<ApiCommand, ApiError> handleCellSet(const rapidjson::Document& cmd);
    Result<ApiCommand, ApiError> handleStateGet(const rapidjson::Document& cmd);
    Result<ApiCommand, ApiError> handleCellGet(const rapidjson::Document& cmd);
    Result<ApiCommand, ApiError> handleGravitySet(const rapidjson::Document& cmd);
    Result<ApiCommand, ApiError> handleReset(const rapidjson::Document& cmd);
};

} // namespace Server
} // namespace DirtSim
