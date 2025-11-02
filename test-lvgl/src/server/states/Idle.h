#pragma once

#include "State.h"

namespace DirtSim {
namespace Server {
namespace State {

/**
 * @brief Idle state - server ready, no active simulation.
 *
 * In this state:
 * - No World exists
 * - Server is listening for commands
 * - Can start simulation with SimRun command
 * - Can exit with Exit command
 */
struct Idle {
    void onEnter(StateMachine& dsm);
    void onExit(StateMachine& dsm);

    Any onEvent(const Api::Exit::Cwc& cwc, StateMachine& dsm);
    // Future: SimRun command will create world and transition to SimRunning.

    static constexpr const char* name() { return "Idle"; }
};

} // namespace State
} // namespace Server
} // namespace DirtSim
