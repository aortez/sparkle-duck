#pragma once

#include "State.h"
#include "../events/Pause.h"
#include "../events/QuitApplication.h"

namespace DirtSim {
namespace Ui {

class StateMachine;

namespace State {

struct SimRunning {
    void onEnter(StateMachine& sm);
    State::Any onEvent(const PauseCommand& cmd, StateMachine& sm);
    State::Any onEvent(const QuitApplicationCommand& cmd, StateMachine& sm);
    static constexpr const char* name() { return "SimRunning"; }
};

} // namespace State
} // namespace Ui
} // namespace DirtSim
