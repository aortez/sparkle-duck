#include "../StateMachine.h"
#include "State.h"
#include <spdlog/spdlog.h>

namespace DirtSim {
namespace Server {
namespace State {

void MainMenu::onEnter(StateMachine& /*dsm*/)
{
    spdlog::info("Server::MainMenu: Entered (headless, no UI)");
}

void MainMenu::onExit(StateMachine& /*dsm*/)
{
    spdlog::info("Server::MainMenu: Exited");
}

State::Any MainMenu::onEvent(const StartSimulationCommand& /*cmd*/, StateMachine& /*dsm*/)
{
    spdlog::info("MainMenu: Starting simulation");
    
    // UI will be created by SimRunning state.
    return SimRunning{};
}

State::Any MainMenu::onEvent(const OpenConfigCommand& /*cmd*/, StateMachine& /*dsm. */)
{
    spdlog::info("MainMenu: Opening configuration");
    return Config{};
}

State::Any MainMenu::onEvent(const SelectMaterialCommand& cmd, StateMachine& dsm)
{
    dsm.getSharedState().setSelectedMaterial(cmd.material);
    spdlog::debug("MainMenu: Selected material {}", static_cast<int>(cmd.material));
    return *this;
}

} // namespace State
} // namespace Server
} // namespace DirtSim