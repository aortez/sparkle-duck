#pragma once

#include "StateForward.h"
#include "server/Event.h"

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
    Any onEvent(const Api::SimRun::Cwc& cwc, StateMachine& dsm);

    static constexpr const char* name() { return "Idle"; }
};

} // namespace State
} // namespace Server
} // namespace DirtSim
