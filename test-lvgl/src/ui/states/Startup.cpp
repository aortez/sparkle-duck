#include "Startup.h"
#include "MainMenu.h"
#include "../StateMachine.h"
#include <spdlog/spdlog.h>

namespace DirtSim {
namespace Ui {
namespace State {

State::Any Startup::onEvent(const InitCompleteEvent& /*evt*/, StateMachine& sm)
{
    spdlog::info("Ui::Startup: Initializing UI");

    // TODO: Connect to WebSocket server.
    // sm.connectToServer("ws://localhost:8080");

    spdlog::info("Ui::Startup: Init complete, transitioning to MainMenu");
    return MainMenu{};
}

std::string getCurrentStateName(const Any& state)
{
    return std::visit([](auto&& s) { return std::string(s.name()); }, state);
}

} // namespace State
} // namespace Ui
} // namespace DirtSim
