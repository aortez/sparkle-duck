#include "CoreControls.h"
#include "server/api/Reset.h"
#include "server/api/WorldResize.h"
#include "ui/state-machine/EventSink.h"
#include "ui/state-machine/api/DrawDebugToggle.h"
#include "ui/state-machine/api/Exit.h"
#include "ui/state-machine/api/PixelRendererToggle.h"
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

    // Pixel Renderer toggle.
    pixelRendererSwitch_ = LVGLBuilder::labeledSwitch(container_)
                               .label("Pixel Renderer")
                               .initialState(false)
                               .callback(onPixelRendererToggled, this)
                               .buildOrLog();

    // World Size toggle slider.
    auto worldSizeBuilder = LVGLBuilder::toggleSlider(container_)
                                .label("World Size")
                                .range(1, 150)  // Max world size
                                .defaultValue(1)  // When off, defaults to 1
                                .value(28)        // Initial value when on
                                .sliderWidth(LV_PCT(85))
                                .valueFormat("%.0f")  // Format as floating point with 0 decimals
                                .valueScale(1.0)      // No scaling needed
                                .initiallyEnabled(true)  // Start with toggle on to show current size
                                .onToggle(onWorldSizeToggled, this)
                                .onSliderChange(onWorldSizeChanged, this);

    worldSizeContainer_ = worldSizeBuilder.buildOrLog();
    if (worldSizeContainer_) {
        // Find the switch and slider within the container
        // The ToggleSliderBuilder creates them as children
        uint32_t child_cnt = lv_obj_get_child_count(worldSizeContainer_);
        for (uint32_t i = 0; i < child_cnt; i++) {
            lv_obj_t* child = lv_obj_get_child(worldSizeContainer_, i);
            if (lv_obj_check_type(child, &lv_switch_class)) {
                worldSizeSwitch_ = child;
                spdlog::debug("CoreControls: Found world size switch");
            } else if (lv_obj_check_type(child, &lv_slider_class)) {
                worldSizeSlider_ = child;
                spdlog::debug("CoreControls: Found world size slider");
            }
        }

        if (!worldSizeSwitch_) {
            spdlog::error("CoreControls: Failed to find world size switch in container");
        }
        if (!worldSizeSlider_) {
            spdlog::error("CoreControls: Failed to find world size slider in container");
        } else {
            // Add RELEASED event handler to the slider for throttling
            // The VALUE_CHANGED event is already handled by the ToggleSliderBuilder
            lv_obj_add_event_cb(worldSizeSlider_, onWorldSizeChanged, LV_EVENT_RELEASED, this);
        }
    }

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

    // Queue UI-local exit event (works in all states, including Disconnected).
    UiApi::Exit::Cwc cwc;
    cwc.callback = [](auto&&) {}; // No response needed.
    self->eventSink_.queueEvent(cwc);
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

void CoreControls::onPixelRendererToggled(lv_event_t* e)
{
    CoreControls* self = static_cast<CoreControls*>(lv_event_get_user_data(e));
    if (!self) return;
    lv_obj_t* switch_obj = static_cast<lv_obj_t*>(lv_event_get_target(e));
    bool enabled = lv_obj_has_state(switch_obj, LV_STATE_CHECKED);

    spdlog::info("CoreControls: Pixel renderer toggled to {}", enabled ? "ON" : "OFF");

    // Queue UI-local pixel renderer toggle event.
    UiApi::PixelRendererToggle::Cwc cwc;
    cwc.command.enabled = enabled;
    cwc.callback = [](auto&&) {}; // No response needed.
    self->eventSink_.queueEvent(cwc);
}

void CoreControls::onWorldSizeToggled(lv_event_t* e)
{
    CoreControls* self = static_cast<CoreControls*>(lv_event_get_user_data(e));
    if (!self) {
        spdlog::error("CoreControls: onWorldSizeToggled called with null self");
        return;
    }

    lv_obj_t* switch_obj = static_cast<lv_obj_t*>(lv_event_get_target(e));
    bool enabled = lv_obj_has_state(switch_obj, LV_STATE_CHECKED);

    spdlog::info("CoreControls: World size toggle switched to {}", enabled ? "ON" : "OFF");

    // When toggled off, the slider defaults to value 1
    // When toggled on, it uses the slider's current value
    if (!enabled) {
        // Send resize command with size 1x1 (minimum world size)
        Api::WorldResize::Command cmd;
        cmd.width = 1;
        cmd.height = 1;
        nlohmann::json j = cmd.toJson();
        j["command"] = "world_resize";
        self->wsClient_->send(j.dump());
        spdlog::info("CoreControls: Resizing world to 1x1 (toggle off)");
    } else {
        // Get the current slider value and resize to that
        if (!self->worldSizeSlider_) {
            spdlog::error("CoreControls: worldSizeSlider_ is null!");
            // Use default value if slider is not available
            Api::WorldResize::Command cmd;
            cmd.width = 28;
            cmd.height = 28;
            nlohmann::json j = cmd.toJson();
            j["command"] = "world_resize";
            self->wsClient_->send(j.dump());
            spdlog::info("CoreControls: Resizing world to 28x28 (default, slider unavailable)");
        } else {
            int32_t value = lv_slider_get_value(self->worldSizeSlider_);
            Api::WorldResize::Command cmd;
            cmd.width = value;
            cmd.height = value;
            nlohmann::json j = cmd.toJson();
            j["command"] = "world_resize";
            self->wsClient_->send(j.dump());
            spdlog::info("CoreControls: Resizing world to {}x{} (toggle on)", value, value);
        }
    }
}

void CoreControls::onWorldSizeChanged(lv_event_t* e)
{
    // Get the slider object first
    lv_obj_t* slider = static_cast<lv_obj_t*>(lv_event_get_target(e));

    // Get CoreControls from the slider's user data, not the event's user data
    CoreControls* self = static_cast<CoreControls*>(lv_obj_get_user_data(slider));
    if (!self) {
        spdlog::error("CoreControls: onWorldSizeChanged called with null self");
        return;
    }

    int32_t value = lv_slider_get_value(slider);

    // Throttle resize commands - only send on value release, not while dragging
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_VALUE_CHANGED) {
        // Store the pending value but don't send command yet
        self->pendingWorldSize_ = value;
        return;
    } else if (code != LV_EVENT_RELEASED) {
        // Only process RELEASED events
        return;
    }

    // Use pending value if set, otherwise current value
    if (self->pendingWorldSize_ > 0) {
        value = self->pendingWorldSize_;
        self->pendingWorldSize_ = 0;
    }

    // Only send resize if the toggle is enabled
    if (self->worldSizeSwitch_ && lv_obj_has_state(self->worldSizeSwitch_, LV_STATE_CHECKED)) {
        spdlog::info("CoreControls: World size slider released at {}", value);

        // Send WorldResize API command
        Api::WorldResize::Command cmd;
        cmd.width = value;
        cmd.height = value;
        nlohmann::json j = cmd.toJson();
        j["command"] = "world_resize";
        self->wsClient_->send(j.dump());
        spdlog::info("CoreControls: Resizing world to {}x{}", value, value);
    }
}

} // namespace Ui
} // namespace DirtSim
