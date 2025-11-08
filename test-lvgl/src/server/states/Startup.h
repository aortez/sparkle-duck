#pragma once

#include "StateForward.h"
#include "server/Event.h"

namespace DirtSim {
namespace Server {
namespace State {

/**
 * @brief Initial startup state - loading resources and initializing systems.
 */
struct Startup {
    State::Any onEvent(const InitCompleteEvent& evt, StateMachine& dsm);

    static constexpr const char* name() { return "Startup"; }
};

} // namespace State
} // namespace Server
} // namespace DirtSim
