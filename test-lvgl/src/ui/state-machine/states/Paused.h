#pragma once

#include "StateForward.h"
#include "ui/state-machine/Event.h"
#include <lvgl/lvgl.h>
#include <memory>

namespace DirtSim {

struct WorldData;

namespace Ui {
namespace State {

/**
 * @brief Paused state - simulation stopped but world still displayed.
 *
 * Shows an overlay with Resume, Stop, and Quit buttons.
 */
struct Paused {
    std::unique_ptr<WorldData> worldData; // Preserve world data while paused.

    // UI elements for pause overlay.
    lv_obj_t* overlay_ = nullptr;
    lv_obj_t* resumeButton_ = nullptr;
    lv_obj_t* stopButton_ = nullptr;
    lv_obj_t* quitButton_ = nullptr;

    void onEnter(StateMachine& sm);
    void onExit(StateMachine& sm);

    Any onEvent(const UiApi::Exit::Cwc& cwc, StateMachine& sm);
    Any onEvent(const UiApi::MouseDown::Cwc& cwc, StateMachine& sm);
    Any onEvent(const UiApi::MouseMove::Cwc& cwc, StateMachine& sm);
    Any onEvent(const UiApi::MouseUp::Cwc& cwc, StateMachine& sm);
    Any onEvent(const UiApi::SimRun::Cwc& cwc, StateMachine& sm);
    Any onEvent(const UiApi::SimStop::Cwc& cwc, StateMachine& sm);
    Any onEvent(const ServerDisconnectedEvent& evt, StateMachine& sm);

    static constexpr const char* name() { return "Paused"; }

private:
    static void onResumeClicked(lv_event_t* e);
    static void onStopClicked(lv_event_t* e);
    static void onQuitClicked(lv_event_t* e);
};

} // namespace State
} // namespace Ui
} // namespace DirtSim
