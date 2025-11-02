#pragma once

#include "State.h"
#include "../events/QuitApplication.h"
#include "../events/StartSimulation.h"

namespace DirtSim {
namespace Ui {

class StateMachine;

namespace State {

struct MainMenu {
    State::Any onEvent(const StartSimulationCommand& cmd, StateMachine& sm);
    State::Any onEvent(const QuitApplicationCommand& cmd, StateMachine& sm);
    static constexpr const char* name() { return "MainMenu"; }
};

} // namespace State
} // namespace Ui
} // namespace DirtSim
