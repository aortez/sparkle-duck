#include "../StateMachine.h"
#include "State.h"
#include <spdlog/spdlog.h>

namespace DirtSim {
namespace Server {
namespace State {

void Paused::onEnter(StateMachine& /*dsm*/)
{
    spdlog::info("Paused: Simulation paused (World preserved)");
}

void Paused::onExit(StateMachine& /*dsm*/)
{
    spdlog::info("Paused: Exiting");
}

State::Any Paused::onEvent(const Api::Exit::Cwc& cwc, StateMachine& /*dsm*/)
{
    spdlog::info("Paused: Exit command received, shutting down");

    // Send success response.
    cwc.sendResponse(Api::Exit::Response::okay(std::monostate{}));

    // Transition to Shutdown state (Shutdown.onEnter will set shouldExit flag).
    return Shutdown{};
}

} // namespace State
} // namespace Server
} // namespace DirtSim
