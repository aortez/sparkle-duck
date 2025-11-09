#pragma once

// This file aggregates all UI state definitions.
// Each state has its own header file for better organization.

#include "Disconnected.h"
#include "Paused.h"
#include "Shutdown.h"
#include "SimRunning.h"
#include "StartMenu.h"
#include "Startup.h"
#include "StateForward.h"

namespace DirtSim {
namespace Ui {
namespace State {

/**
 * @brief Get the name of the current state.
 * Requires complete state definitions, so defined here after all includes.
 */
inline std::string getCurrentStateName(const Any& state)
{
    return std::visit([](const auto& s) { return std::string(s.name()); }, state);
}

} // namespace State
} // namespace Ui
} // namespace DirtSim
