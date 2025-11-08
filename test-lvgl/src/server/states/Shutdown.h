#pragma once

#include "StateForward.h"
#include "server/Event.h"

namespace DirtSim {
namespace Server {
namespace State {

/**
 * @brief Shutdown state - cleanup and exit.
 */
struct Shutdown {
    void onEnter(StateMachine& dsm);

    static constexpr const char* name() { return "Shutdown"; }
};

} // namespace State
} // namespace Server
} // namespace DirtSim
