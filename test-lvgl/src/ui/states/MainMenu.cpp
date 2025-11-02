#include "MainMenu.h"
#include "SimRunning.h"
#include "Shutdown.h"
#include "../StateMachine.h"
#include <spdlog/spdlog.h>

namespace DirtSim {
namespace Ui {
namespace State {

State::Any MainMenu::onEvent(const StartSimulationCommand& /*cmd*/, StateMachine& sm)
{
    spdlog::info("Ui::MainMenu: Start button pressed, requesting server to start");

    // TODO: Implement blocking server call.
    // auto response = sm.sendToServerBlocking(Server::StartSimulationCommand{timestep, duration});
    //
    // if (response.isOk()) {
    //     spdlog::info("Ui::MainMenu: Server confirmed start");
    //     return SimRunning{};
    // }
    //
    // spdlog::error("Ui::MainMenu: Server refused to start: {}", response.error());
    // return *this;

    // Temporary: transition immediately.
    return SimRunning{};
}

State::Any MainMenu::onEvent(const QuitApplicationCommand& /*cmd*/, StateMachine& /*sm*/)
{
    spdlog::info("Ui::MainMenu: Quit requested");
    return Shutdown{};
}

} // namespace State
} // namespace Ui
} // namespace DirtSim
