#pragma once

#include "State.h"
#include "../events/InitComplete.h"

namespace DirtSim {
namespace Ui {

class StateMachine;

namespace State {

struct Startup {
    State::Any onEvent(const InitCompleteEvent& evt, StateMachine& sm);
    static constexpr const char* name() { return "Startup"; }
};

} // namespace State
} // namespace Ui
} // namespace DirtSim
