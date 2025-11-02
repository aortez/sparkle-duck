#pragma once

#include "State.h"
#include "../events/Pause.h"
#include "../events/QuitApplication.h"

namespace DirtSim {
namespace Ui {

class StateMachine;

namespace State {

struct Paused {
    void onEnter(StateMachine& sm);
    State::Any onEvent(const ResumeCommand& cmd, StateMachine& sm);
    State::Any onEvent(const QuitApplicationCommand& cmd, StateMachine& sm);
    static constexpr const char* name() { return "Paused"; }
};

} // namespace State
} // namespace Ui
} // namespace DirtSim
