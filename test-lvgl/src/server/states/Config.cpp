#include "../StateMachine.h"
#include "State.h"
#include <spdlog/spdlog.h>

namespace DirtSim {
namespace Server {
namespace State {

void Config::onEnter(StateMachine& /*dsm*/)
{
    spdlog::info("Server::Config: Entered (headless, no UI)");
    // TODO: Configuration happens via API commands, not UI.
}

void Config::onExit(StateMachine& /*dsm*/)
{
    spdlog::info("Server::Config: Exited");
}

State::Any Config::onEvent(const StartSimulationCommand& /*cmd*/, StateMachine& /*dsm*/)
{
    spdlog::info("Server::Config: Start requested, returning to MainMenu");
    return MainMenu{};
}

} // namespace State
} // namespace Server
} // namespace DirtSim
