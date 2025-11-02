#include "Paused.h"
#include "MainMenu.h"
#include "Shutdown.h"
#include "../StateMachine.h"
#include <spdlog/spdlog.h>

namespace DirtSim {
namespace Ui {
namespace State {

void Paused::onEnter(StateMachine& /*sm*/)
{
    spdlog::info("Ui::Paused: Entered, server already stopped");
}

State::Any Paused::onEvent(const ResumeCommand& /*cmd*/, StateMachine& /*sm*/)
{
    spdlog::info("Ui::Paused: Resume pressed, going back to MainMenu for now");
    // TODO: Implement resume flow.
    return MainMenu{};
}

State::Any Paused::onEvent(const QuitApplicationCommand& /*cmd*/, StateMachine& /*sm*/)
{
    spdlog::info("Ui::Paused: Quit requested");
    return Shutdown{};
}

} // namespace State
} // namespace Ui
} // namespace DirtSim
