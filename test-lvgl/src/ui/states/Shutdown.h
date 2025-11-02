#pragma once

#include "State.h"

namespace DirtSim {
namespace Ui {

class StateMachine;

namespace State {

struct Shutdown {
    void onEnter(StateMachine& sm);
    static constexpr const char* name() { return "Shutdown"; }
};

} // namespace State
} // namespace Ui
} // namespace DirtSim
