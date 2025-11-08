#include "ui/state-machine/StateMachine.h"
#include "State.h"
#include <spdlog/spdlog.h>

namespace DirtSim {
namespace Ui {
namespace State {

void Shutdown::onEnter(StateMachine& sm)
{
    spdlog::info("Shutdown: Performing cleanup");

    // TODO: Disconnect from DSSM server if connected.
    // TODO: Close WebSocket server if running.
    // TODO: Clean up LVGL resources.

    // Set exit flag to signal main loop to exit.
    spdlog::info("Shutdown: Setting shouldExit flag to true");
    sm.setShouldExit(true);

    spdlog::info("Shutdown: Cleanup complete, shouldExit={}", sm.shouldExit());
}

} // namespace State
} // namespace Ui
} // namespace DirtSim
