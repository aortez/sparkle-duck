#include "State.h"
#include "ui/state-machine/StateMachine.h"
#include <spdlog/spdlog.h>

namespace DirtSim {
namespace Ui {
namespace State {

State::Any Startup::onEvent(const InitCompleteEvent& /*evt*/, StateMachine& /*sm*/)
{
    spdlog::info("Startup: LVGL and display systems initialized");
    spdlog::info("Startup: Transitioning to Disconnected (no server connection)");

    // Transition to Disconnected state (show connection UI).
    return Disconnected{};
}

} // namespace State
} // namespace Ui
} // namespace DirtSim
