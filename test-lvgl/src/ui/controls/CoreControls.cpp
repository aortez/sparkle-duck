#include "CoreControls.h"
#include "server/api/Exit.h"
#include "server/api/Reset.h"
#include "ui/state-machine/EventSink.h"
#include "ui/state-machine/api/DrawDebugToggle.h"
#include "ui/state-machine/network/WebSocketClient.h"
#include "ui/ui_builders/LVGLBuilder.h"
#include <lvgl/src/misc/lv_palette.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

namespace DirtSim {
namespace Ui {

CoreControls::CoreControls(lv_obj_t* container, WebSocketClient* wsClient, EventSink& eventSink)
    : container_(container), wsClient_(wsClient), eventSink_(eventSink)
{
    // Quit button.
    quitButton_ = lv_btn_create(container_);
    lv_obj_set_width(quitButton_, LV_PCT(90));
    lv_obj_set_style_bg_color(quitButton_, lv_palette_main(LV_PALETTE_RED), 0);
    lv_obj_t* quitLabel = lv_label_create(quitButton_);
    lv_label_set_text(quitLabel, "Quit");
    lv_obj_center(quitLabel);
    lv_obj_add_event_cb(quitButton_, onQuitClicked, LV_EVENT_CLICKED, this);

    // Reset button.
    resetButton_ = lv_btn_create(container_);
    lv_obj_set_width(resetButton_, LV_PCT(90));
    lv_obj_set_style_bg_color(resetButton_, lv_palette_main(LV_PALETTE_ORANGE), 0);
    lv_obj_t* resetLabel = lv_label_create(resetButton_);
    lv_label_set_text(resetLabel, "Reset");
    lv_obj_center(resetLabel);
    lv_obj_add_event_cb(resetButton_, onResetClicked, LV_EVENT_CLICKED, this);

    // Add spacing after buttons.
    lv_obj_t* spacer1 = lv_obj_create(container_);
    lv_obj_set_size(spacer1, LV_PCT(100), 10);
    lv_obj_set_style_bg_opa(spacer1, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(spacer1, 0, 0);

    // Stats display.
    statsLabel_ = lv_label_create(container_);
    lv_label_set_text(statsLabel_, "Server: -- FPS");
    lv_obj_set_style_text_font(statsLabel_, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(statsLabel_, lv_color_white(), 0);

    statsLabelUI_ = lv_label_create(container_);
    lv_label_set_text(statsLabelUI_, "UI: -- FPS");
    lv_obj_set_style_text_font(statsLabelUI_, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(statsLabelUI_, lv_color_white(), 0);

    // Add spacing after stats labels.
    lv_obj_t* spacer2 = lv_obj_create(container_);
    lv_obj_set_size(spacer2, LV_PCT(100), 10);
    lv_obj_set_style_bg_opa(spacer2, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(spacer2, 0, 0);

    // Debug toggle.
    debugSwitch_ = LVGLBuilder::labeledSwitch(container_)
                       .label("Debug Draw")
                       .initialState(false)
                       .callback(onDebugToggled, this)
                       .buildOrLog();

    spdlog::info("CoreControls: Initialized");
}

CoreControls::~CoreControls()
{
    // No manual cleanup needed - LVGL automatically destroys callbacks when widgets are destroyed.
    spdlog::info("CoreControls: Destroyed");
}

void CoreControls::updateStats(double serverFPS, double uiFPS)
{
    if (statsLabel_) {
        char buf[64];
        snprintf(buf, sizeof(buf), "Server: %.1f FPS", serverFPS);
        lv_label_set_text(statsLabel_, buf);
    }

    if (statsLabelUI_) {
        char buf[64];
        snprintf(buf, sizeof(buf), "UI: %.1f FPS", uiFPS);
        lv_label_set_text(statsLabelUI_, buf);
    }
}

void CoreControls::onQuitClicked(lv_event_t* e)
{
    CoreControls* self = static_cast<CoreControls*>(lv_event_get_user_data(e));
    if (!self) return;

    spdlog::info("CoreControls: Quit button clicked");

    // Send exit command to server.
    Api::Exit::Command cmd;
    nlohmann::json j = cmd.toJson();
    j["command"] = "exit";
    self->wsClient_->send(j.dump());
}

void CoreControls::onResetClicked(lv_event_t* e)
{
    CoreControls* self = static_cast<CoreControls*>(lv_event_get_user_data(e));
    if (!self) return;

    spdlog::info("CoreControls: Reset button clicked");

    // Send reset command to server.
    Api::Reset::Command cmd;
    nlohmann::json j = cmd.toJson();
    j["command"] = "reset";
    self->wsClient_->send(j.dump());
}

void CoreControls::onDebugToggled(lv_event_t* e)
{
    CoreControls* self = static_cast<CoreControls*>(lv_event_get_user_data(e));
    if (!self) return;
    lv_obj_t* switch_obj = static_cast<lv_obj_t*>(lv_event_get_target(e));
    bool enabled = lv_obj_has_state(switch_obj, LV_STATE_CHECKED);

    spdlog::info("CoreControls: Debug draw toggled to {}", enabled ? "ON" : "OFF");

    // Queue UI-local debug toggle event.
    UiApi::DrawDebugToggle::Cwc cwc;
    cwc.command.enabled = enabled;
    cwc.callback = [](auto&&) {}; // No response needed.
    self->eventSink_.queueEvent(cwc);
}

} // namespace Ui
} // namespace DirtSim
