#pragma once

#include <string>
#include <zpp_bits.h>

namespace DirtSim {

struct ApiError {
    std::string message;

    // zpp_bits serialization support.
    using serialize = zpp::bits::members<1>;

    ApiError() : message("Unknown error") {}
    explicit ApiError(const std::string& msg) : message(msg) {}
    ApiError(const char* msg) : message(msg) {}
};

} // namespace DirtSim
