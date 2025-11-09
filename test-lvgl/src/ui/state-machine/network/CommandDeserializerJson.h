#pragma once

#include "core/Result.h"
#include "server/api/ApiError.h"
#include "ui/state-machine/api/UiApiCommand.h"
#include <nlohmann/json.hpp>
#include <string>

namespace DirtSim {
namespace Ui {

/**
 * @brief Deserializes JSON command strings into UI API command structs.
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
    Result<UiApiCommand, ApiError> deserialize(const std::string& commandJson);
};

} // namespace Ui
} // namespace DirtSim
