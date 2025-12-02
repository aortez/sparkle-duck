#include "State.h"
#include "core/Timers.h"
#include "server/StateMachine.h"
#include "server/scenarios/ScenarioRegistry.h"
#include <spdlog/spdlog.h>

namespace DirtSim {
namespace Server {
namespace State {

void Shutdown::onEnter(StateMachine& dsm)
{
    spdlog::info("Shutdown: Performing cleanup");

    // Don't touch UI here - let the backend loop handle UI cleanup
    // to avoid LVGL rendering conflicts

    // World cleanup is handled by StateMachine destructor.
    // is still using it. It will be cleaned up when StateMachine
    // is destroyed.

    // Set exit flag to signal backend loop and state machine thread to exit.
    spdlog::info("Shutdown: Setting shouldExit flag to true");
    dsm.setShouldExit(true);

    spdlog::info("Shutdown: Cleanup complete, shouldExit={}", dsm.shouldExit());
}

} // namespace State
} // namespace Server
} // namespace DirtSim