#include "State.h"
#include "ui/state-machine/StateMachine.h"
#include <spdlog/spdlog.h>

namespace DirtSim {
namespace Ui {
namespace State {

void Paused::onEnter(StateMachine& sm)
{
    spdlog::info("Paused: Simulation paused, creating overlay");

    // Create semi-transparent overlay on top of the frozen world.
    lv_obj_t* screen = lv_scr_act();
    overlay_ = lv_obj_create(screen);
    lv_obj_set_size(overlay_, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(overlay_, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(overlay_, LV_OPA_50, 0);
    lv_obj_clear_flag(overlay_, LV_OBJ_FLAG_SCROLLABLE);

    // Create centered button container.
    lv_obj_t* buttonContainer = lv_obj_create(overlay_);
    lv_obj_set_size(buttonContainer, 200, 180);
    lv_obj_center(buttonContainer);
    lv_obj_set_style_bg_color(buttonContainer, lv_color_hex(0x333333), 0);
    lv_obj_set_style_bg_opa(buttonContainer, LV_OPA_90, 0);
    lv_obj_set_style_radius(buttonContainer, 10, 0);
    lv_obj_set_style_pad_all(buttonContainer, 15, 0);
    lv_obj_set_flex_flow(buttonContainer, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(
        buttonContainer, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(buttonContainer, LV_OBJ_FLAG_SCROLLABLE);

    // "PAUSED" label.
    lv_obj_t* pausedLabel = lv_label_create(buttonContainer);
    lv_label_set_text(pausedLabel, "PAUSED");
    lv_obj_set_style_text_font(pausedLabel, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(pausedLabel, lv_color_hex(0xFFFFFF), 0);

    // Resume button (green).
    resumeButton_ = lv_btn_create(buttonContainer);
    lv_obj_set_size(resumeButton_, 160, 40);
    lv_obj_set_style_bg_color(resumeButton_, lv_palette_main(LV_PALETTE_GREEN), 0);
    lv_obj_set_user_data(resumeButton_, &sm);
    lv_obj_add_event_cb(resumeButton_, onResumeClicked, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* resumeLabel = lv_label_create(resumeButton_);
    lv_label_set_text(resumeLabel, "Resume");
    lv_obj_center(resumeLabel);

    // Stop button (orange) - returns to start menu.
    stopButton_ = lv_btn_create(buttonContainer);
    lv_obj_set_size(stopButton_, 160, 40);
    lv_obj_set_style_bg_color(stopButton_, lv_palette_main(LV_PALETTE_ORANGE), 0);
    lv_obj_set_user_data(stopButton_, &sm);
    lv_obj_add_event_cb(stopButton_, onStopClicked, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* stopLabel = lv_label_create(stopButton_);
    lv_label_set_text(stopLabel, "Stop");
    lv_obj_center(stopLabel);

    // Quit button (red).
    quitButton_ = lv_btn_create(buttonContainer);
    lv_obj_set_size(quitButton_, 160, 40);
    lv_obj_set_style_bg_color(quitButton_, lv_palette_main(LV_PALETTE_RED), 0);
    lv_obj_set_user_data(quitButton_, &sm);
    lv_obj_add_event_cb(quitButton_, onQuitClicked, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* quitLabel = lv_label_create(quitButton_);
    lv_label_set_text(quitLabel, "Quit");
    lv_obj_center(quitLabel);

    spdlog::info("Paused: Created overlay with Resume/Stop/Quit buttons");
}

void Paused::onExit(StateMachine& /*sm*/)
{
    spdlog::info("Paused: Exiting, cleaning up overlay");

    if (overlay_) {
        lv_obj_del(overlay_);
        overlay_ = nullptr;
        resumeButton_ = nullptr;
        stopButton_ = nullptr;
        quitButton_ = nullptr;
    }
}

void Paused::onResumeClicked(lv_event_t* e)
{
    auto* sm = static_cast<StateMachine*>(
        lv_obj_get_user_data(static_cast<lv_obj_t*>(lv_event_get_target(e))));
    if (!sm) return;

    spdlog::info("Paused: Resume button clicked");

    UiApi::SimRun::Cwc cwc;
    cwc.callback = [](auto&&) {};
    sm->queueEvent(cwc);
}

void Paused::onStopClicked(lv_event_t* e)
{
    auto* sm = static_cast<StateMachine*>(
        lv_obj_get_user_data(static_cast<lv_obj_t*>(lv_event_get_target(e))));
    if (!sm) return;

    spdlog::info("Paused: Stop button clicked");

    UiApi::SimStop::Cwc cwc;
    cwc.callback = [](auto&&) {};
    sm->queueEvent(cwc);
}

void Paused::onQuitClicked(lv_event_t* e)
{
    auto* sm = static_cast<StateMachine*>(
        lv_obj_get_user_data(static_cast<lv_obj_t*>(lv_event_get_target(e))));
    if (!sm) return;

    spdlog::info("Paused: Quit button clicked");

    UiApi::Exit::Cwc cwc;
    cwc.callback = [](auto&&) {};
    sm->queueEvent(cwc);
}

State::Any Paused::onEvent(const UiApi::Exit::Cwc& cwc, StateMachine& /*sm*/)
{
    spdlog::info("Paused: Exit command received, shutting down");

    cwc.sendResponse(UiApi::Exit::Response::okay(std::monostate{}));

    return Shutdown{};
}

State::Any Paused::onEvent(const UiApi::MouseDown::Cwc& cwc, StateMachine& /*sm*/)
{
    spdlog::debug("Paused: Mouse down at ({}, {})", cwc.command.pixelX, cwc.command.pixelY);

    cwc.sendResponse(UiApi::MouseDown::Response::okay(std::monostate{}));
    return Paused{ std::move(worldData) };
}

State::Any Paused::onEvent(const UiApi::MouseMove::Cwc& cwc, StateMachine& /*sm*/)
{
    spdlog::debug("Paused: Mouse move at ({}, {})", cwc.command.pixelX, cwc.command.pixelY);

    cwc.sendResponse(UiApi::MouseMove::Response::okay(std::monostate{}));
    return Paused{ std::move(worldData) };
}

State::Any Paused::onEvent(const UiApi::MouseUp::Cwc& cwc, StateMachine& /*sm*/)
{
    spdlog::debug("Paused: Mouse up at ({}, {})", cwc.command.pixelX, cwc.command.pixelY);

    cwc.sendResponse(UiApi::MouseUp::Response::okay(std::monostate{}));
    return Paused{ std::move(worldData) };
}

State::Any Paused::onEvent(const UiApi::SimRun::Cwc& cwc, StateMachine& /*sm*/)
{
    spdlog::info("Paused: SimRun command received, resuming simulation");

    cwc.sendResponse(UiApi::SimRun::Response::okay({ true }));

    SimRunning newState;
    newState.worldData = std::move(worldData);
    return newState;
}

State::Any Paused::onEvent(const UiApi::SimStop::Cwc& cwc, StateMachine& /*sm*/)
{
    spdlog::info("Paused: SimStop command received, returning to start menu");

    cwc.sendResponse(UiApi::SimStop::Response::okay({ true }));

    // Discard world data and return to start menu.
    return StartMenu{};
}

State::Any Paused::onEvent(const ServerDisconnectedEvent& evt, StateMachine& /*sm*/)
{
    spdlog::warn("Paused: Server disconnected (reason: {})", evt.reason);
    spdlog::info("Paused: Transitioning to Disconnected");

    return Disconnected{};
}

} // namespace State
} // namespace Ui
} // namespace DirtSim
