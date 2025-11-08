#pragma once

#include "StateForward.h"
#include "ui/state-machine/Event.h"

namespace DirtSim {
namespace Ui {
namespace State {

/**
 * @brief Initial startup state - initializing LVGL and display systems.
 */
struct Startup {
    State::Any onEvent(const InitCompleteEvent& evt, StateMachine& sm);

    static constexpr const char* name() { return "Startup"; }
};

} // namespace State
} // namespace Ui
} // namespace DirtSim
