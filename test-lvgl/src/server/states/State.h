#pragma once

// This file aggregates all server state definitions.
// Each state has its own header file for better organization.

#include "../../core/World.h"  // Must be before SimRunning.h for complete type in unique_ptr.
#include "StateForward.h"
#include "Startup.h"
#include "Idle.h"
#include "SimRunning.h"
#include "SimPaused.h"
#include "Shutdown.h"

namespace DirtSim {
namespace Server {
namespace State {

/**
 * @brief Get the name of the current state.
 * Requires complete state definitions, so defined here after all includes.
 */
inline std::string getCurrentStateName(const Any& state) {
    return std::visit([](const auto& s) { return std::string(s.name()); }, state);
}

} // namespace State
} // namespace Server
} // namespace DirtSim
