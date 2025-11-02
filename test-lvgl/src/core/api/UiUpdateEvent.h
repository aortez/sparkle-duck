#pragma once

#include "../World.h"
#include <chrono>
#include <cstdint>

namespace DirtSim {

struct UiUpdateEvent {
    uint64_t sequenceNum = 0;
    World world;
    uint32_t fps = 0;
    uint64_t stepCount = 0;
    bool isPaused = false;
    std::chrono::steady_clock::time_point timestamp;

    static constexpr const char* name() { return "UiUpdateEvent"; }
};

} // namespace DirtSim
