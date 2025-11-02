#include "SimRunning.h"
#include "Shutdown.h"
#include "../StateMachine.h"
#include <spdlog/spdlog.h>

namespace DirtSim {
namespace Ui {
namespace State {

void SimRunning::onEnter(StateMachine& /*sm*/)
{
    spdlog::info("Ui::SimRunning: Entered simulation running state");
}

State::Any SimRunning::onEvent(const QuitApplicationCommand& /*cmd*/, StateMachine& sm)
{
    spdlog::info("Ui::SimRunning: Quit requested");

    // TODO: Tell server to stop simulation (blocking).
    // sm.sendToServerBlocking(Server::StopSimulationCommand{});

    return Shutdown{};
}

} // namespace State
} // namespace Ui
} // namespace DirtSim
