#pragma once

#include "../../core/api/UiUpdateEvent.h"
#include "CaptureScreenshot.h"
#include "InitComplete.h"
#include "Pause.h"
#include "QuitApplication.h"
#include "StartSimulation.h"
#include <string>
#include <variant>

namespace DirtSim {
namespace Ui {

using Event = std::variant<
    DirtSim::UiUpdateEvent,
    StartSimulationCommand,
    QuitApplicationCommand,
    PauseCommand,
    ResumeCommand,
    CaptureScreenshotCommand,
    InitCompleteEvent
>;

inline std::string getEventName(const Event& event)
{
    return std::visit([](auto&& e) { return std::string(e.name()); }, event);
}

} // namespace Ui
} // namespace DirtSim
