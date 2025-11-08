#include "server/StateMachine.h"
#include "State.h"
#include <spdlog/spdlog.h>

namespace DirtSim {
namespace Server {
namespace State {

void SimPaused::onEnter(StateMachine& /*dsm*/)
{
    spdlog::info("SimPaused: Simulation paused at step {} (World preserved)", previousState.stepCount);
}

void SimPaused::onExit(StateMachine& /*dsm*/)
{
    spdlog::info("SimPaused: Exiting paused state");
}

State::Any SimPaused::onEvent(const Api::Exit::Cwc& cwc, StateMachine& /*dsm*/)
{
    spdlog::info("SimPaused: Exit command received, shutting down");

    // Send success response.
    cwc.sendResponse(Api::Exit::Response::okay(std::monostate{}));

    // Transition to Shutdown state (World destroyed with SimPaused).
    return Shutdown{};
}

// Future handlers:
// - SimRun: Resume with new parameters → std::move(previousState)
// - SimStop: Destroy world and return to Idle
// - Query commands: get_state, get_cell, etc. (access previousState.world)

} // namespace State
} // namespace Server
} // namespace DirtSim
