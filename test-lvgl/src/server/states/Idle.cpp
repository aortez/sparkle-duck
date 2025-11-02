#include "../StateMachine.h"
#include "State.h"
#include <spdlog/spdlog.h>

namespace DirtSim {
namespace Server {
namespace State {

void Idle::onEnter(StateMachine& /*dsm*/)
{
    spdlog::info("Idle: Server ready, waiting for commands (no active World)");
    // Note: World is owned by SimRunning state, not StateMachine.
}

void Idle::onExit(StateMachine& /*dsm*/)
{
    spdlog::info("Idle: Exiting");
}

State::Any Idle::onEvent(const Api::Exit::Cwc& cwc, StateMachine& /*dsm*/)
{
    spdlog::info("Idle: Exit command received, shutting down");

    // Send success response.
    cwc.sendResponse(Api::Exit::Response::okay(std::monostate{}));

    // Transition to Shutdown state (Shutdown.onEnter will set shouldExit flag).
    return Shutdown{};
}

} // namespace State
} // namespace Server
} // namespace DirtSim
