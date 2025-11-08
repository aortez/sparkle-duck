#pragma once

#include "StateForward.h"
#include "ui/state-machine/Event.h"

namespace DirtSim {
namespace Ui {
namespace State {

/**
 * @brief Shutdown state - cleaning up resources and exiting.
 */
struct Shutdown {
    void onEnter(StateMachine& sm);

    static constexpr const char* name() { return "Shutdown"; }
};

} // namespace State
} // namespace Ui
} // namespace DirtSim
