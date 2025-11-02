#include "../../core/WorldEventGenerator.h"
#include "../StateMachine.h"
#include "State.h"
#include <spdlog/spdlog.h>

namespace DirtSim {
namespace State {

void Shutdown::onEnter(DirtSimStateMachine& dsm) {
    spdlog::info("Shutdown: Performing cleanup");

    // Don't touch UI here - let the backend loop handle UI cleanup
    // to avoid LVGL rendering conflicts

    // World cleanup is handled by DirtSimStateMachine destructor.
    // is still using it. It will be cleaned up when DirtSimStateMachine
    // is destroyed.

    // Set exit flag to signal backend loop and state machine thread to exit.
    spdlog::info("Shutdown: Setting shouldExit flag to true");
    dsm.getSharedState().setShouldExit(true);

    spdlog::info("Shutdown: Cleanup complete, shouldExit={}", dsm.getSharedState().getShouldExit());
}

} // namespace State.
} // namespace DirtSim.