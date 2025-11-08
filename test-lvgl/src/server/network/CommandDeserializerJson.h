#pragma once

#include "server/api/ApiCommand.h"
#include <nlohmann/json.hpp>
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
};

} // namespace Server
} // namespace DirtSim
