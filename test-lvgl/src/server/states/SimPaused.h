#pragma once

#include "StateForward.h"
#include "SimRunning.h"  // Need full definition for previousState member.
#include "server/Event.h"

namespace DirtSim {
namespace Server {
namespace State {

/**
 * @brief Paused simulation state - preserves SimRunning context.
 */
struct SimPaused {
    SimRunning previousState;  // Preserves World, stepCount, run parameters.

    void onEnter(StateMachine& dsm);
    void onExit(StateMachine& dsm);

    Any onEvent(const Api::Exit::Cwc& cwc, StateMachine& dsm);

    static constexpr const char* name() { return "SimPaused"; }
};

} // namespace State
} // namespace Server
} // namespace DirtSim
