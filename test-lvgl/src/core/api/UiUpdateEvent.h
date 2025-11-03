#pragma once

#include "../WorldData.h"
#include <chrono>
#include <cstdint>

namespace DirtSim {

struct UiUpdateEvent {
    uint64_t sequenceNum = 0;
    WorldData worldData;  // Just the data, no physics calculators needed for rendering.
    uint32_t fps = 0;
    uint64_t stepCount = 0;
    bool isPaused = false;
    std::chrono::steady_clock::time_point timestamp;

    static constexpr const char* name() { return "UiUpdateEvent"; }
};

} // namespace DirtSim
