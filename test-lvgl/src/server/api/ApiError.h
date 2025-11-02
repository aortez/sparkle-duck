#pragma once

#include <string>

namespace DirtSim {

struct ApiError {
    std::string message;

    ApiError() : message("Unknown error") {}
    explicit ApiError(const std::string& msg) : message(msg) {}
    ApiError(const char* msg) : message(msg) {}
};

} // namespace DirtSim
