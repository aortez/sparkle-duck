#pragma once

#include "StateForward.h"
#include "../Event.h"
#include <memory>

namespace DirtSim {

class World;

namespace Ui {
namespace State {

/**
 * @brief Paused state - simulation stopped but world still displayed.
 */
struct Paused {
    std::unique_ptr<World> world;  // Preserve world state while paused.

    void onEnter(StateMachine& sm);
    void onExit(StateMachine& sm);

    Any onEvent(const UiApi::Exit::Cwc& cwc, StateMachine& sm);
    Any onEvent(const UiApi::MouseDown::Cwc& cwc, StateMachine& sm);
    Any onEvent(const UiApi::MouseMove::Cwc& cwc, StateMachine& sm);
    Any onEvent(const UiApi::MouseUp::Cwc& cwc, StateMachine& sm);
    Any onEvent(const UiApi::Screenshot::Cwc& cwc, StateMachine& sm);
    Any onEvent(const UiApi::SimRun::Cwc& cwc, StateMachine& sm);
    Any onEvent(const ServerDisconnectedEvent& evt, StateMachine& sm);

    static constexpr const char* name() { return "Paused"; }
};

} // namespace State
} // namespace Ui
} // namespace DirtSim
