#include "Shutdown.h"
#include "../StateMachine.h"
#include "../SimulatorUI.h"
#include <spdlog/spdlog.h>

namespace DirtSim {
namespace Ui {
namespace State {

void Shutdown::onEnter(StateMachine& sm)
{
    spdlog::info("Ui::Shutdown: Taking exit screenshot");
    SimulatorUI::takeExitScreenshot();

    spdlog::info("Ui::Shutdown: Setting exit flag");
    sm.setShouldExit(true);
}

} // namespace State
} // namespace Ui
} // namespace DirtSim
