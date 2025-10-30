#pragma once

#include "../Result.h"
#include <string>

/**
 * Error wrapper to avoid Result<std::string, std::string> ambiguity.
 */
struct CommandError {
    std::string message;

    CommandError() : message("Unknown error") {}
    explicit CommandError(const std::string& msg) : message(msg) {}
    CommandError(const char* msg) : message(msg) {}
};

/**
 * Type alias for command processor results.
 * Success: JSON response string
 * Error: Error message wrapped in CommandError
 */
using CommandResult = Result<std::string, CommandError>;
