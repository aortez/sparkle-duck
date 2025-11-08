#include "core/World.h"
#include "core/WorldEventGenerator.h"
#include "server/StateMachine.h"
#include "State.h"
#include <spdlog/spdlog.h>

namespace DirtSim {
namespace Server {
namespace State {

State::Any Startup::onEvent(const InitCompleteEvent& /*evt*/, StateMachine& /*dsm*/)
{
    spdlog::info("Startup: Initialization complete");
    spdlog::info("Startup: Transitioning to Idle (server ready, no active simulation)");

    // Transition to Idle state (no World, waiting for SimRun command).
    return Idle{};
}

} // namespace State
} // namespace Server
} // namespace DirtSim