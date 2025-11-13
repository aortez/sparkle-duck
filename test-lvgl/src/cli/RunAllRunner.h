#pragma once

#include "core/Result.h"
#include <string>
#include <variant>

namespace DirtSim {
namespace Client {

/**
 * @brief Launches server and UI, monitors until UI exits, then shuts down server.
 * @param serverPath Path to sparkle-duck-server executable.
 * @param uiPath Path to sparkle-duck-ui executable.
 * @return Ok on success, error message on failure.
 */
Result<std::monostate, std::string> runAll(
    const std::string& serverPath, const std::string& uiPath);

} // namespace Client
} // namespace DirtSim
