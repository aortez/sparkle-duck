#pragma once

#include "StateForward.h"
#include "ui/state-machine/Event.h"
#include <lvgl/lvgl.h>

namespace DirtSim {
namespace Ui {

namespace State {

/**
 * @brief Start menu state - connected to server, ready to start simulation.
 * Shows simulation controls (start, scenario selection, etc.).
 */
struct StartMenu {
    void onEnter(StateMachine& sm);
    void onExit(StateMachine& sm);

    Any onEvent(const FrameReadyNotification& evt, StateMachine& sm);
    Any onEvent(const ServerDisconnectedEvent& evt, StateMachine& sm);
    Any onEvent(const ServerRunningConfirmedEvent& evt, StateMachine& sm);
    Any onEvent(const UiApi::Exit::Cwc& cwc, StateMachine& sm);
    Any onEvent(const UiApi::SimRun::Cwc& cwc, StateMachine& sm);

    static constexpr const char* name() { return "StartMenu"; }

private:
    static void onStartButtonClicked(lv_event_t* e);
    static void onAnimationTimer(lv_timer_t* timer);
    static void onDisplayResized(lv_event_t* e);

    lv_timer_t* animationTimer_ = nullptr;
};

} // namespace State
} // namespace Ui
} // namespace DirtSim
