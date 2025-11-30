#include "CoreControls.h"
#include "server/api/Reset.h"
#include "server/api/WorldResize.h"
#include "ui/rendering/CellRenderer.h"
#include "ui/state-machine/EventSink.h"
#include "ui/state-machine/api/DrawDebugToggle.h"
#include "ui/state-machine/api/Exit.h"
#include "ui/state-machine/api/PixelRendererToggle.h"
#include "ui/state-machine/api/RenderModeSelect.h"
#include "ui/state-machine/network/WebSocketClient.h"
#include "ui/ui_builders/LVGLBuilder.h"
#include <lvgl/src/misc/lv_palette.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

namespace DirtSim {
namespace Ui {

CoreControls::CoreControls(
    lv_obj_t* container, WebSocketClient* wsClient, EventSink& eventSink, RenderMode initialMode)
    : container_(container),
      wsClient_(wsClient),
      eventSink_(eventSink),
      currentRenderMode_(initialMode)
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

    // Stats display.
    statsLabel_ = lv_label_create(container_);
    lv_label_set_text(statsLabel_, "Server: -- FPS");
    lv_obj_set_style_text_font(statsLabel_, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(statsLabel_, lv_color_white(), 0);

    statsLabelUI_ = lv_label_create(container_);
    lv_label_set_text(statsLabelUI_, "UI: -- FPS");
    lv_obj_set_style_text_font(statsLabelUI_, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(statsLabelUI_, lv_color_white(), 0);

    // Debug toggle.
    debugSwitch_ = LVGLBuilder::labeledSwitch(container_)
                       .label("Debug Draw")
                       .initialState(false)
                       .callback(onDebugToggled, this)
                       .buildOrLog();

    // Render Mode dropdown (styled like labeledSwitch).
    lv_obj_t* renderModeContainer = lv_obj_create(container_);
    lv_obj_set_size(renderModeContainer, LV_PCT(90), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(renderModeContainer, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(
        renderModeContainer, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(renderModeContainer, 5, 0);
    lv_obj_set_style_pad_column(renderModeContainer, 8, 0);
    lv_obj_set_style_bg_color(renderModeContainer, lv_color_hex(0x0000FF), 0); // Blue background.
    lv_obj_set_style_bg_opa(renderModeContainer, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(renderModeContainer, 5, 0); // Rounded corners.

    lv_obj_t* renderModeLabel = lv_label_create(renderModeContainer);
    lv_label_set_text(renderModeLabel, "Render Mode:");
    lv_obj_set_style_text_color(renderModeLabel, lv_color_hex(0xFFFFFF), 0);

    renderModeDropdown_ = lv_dropdown_create(renderModeContainer);
    lv_dropdown_set_options(
        renderModeDropdown_, "Adaptive\nSharp\nSmooth\nPixel Perfect\nLVGL Debug");
    lv_dropdown_set_selected(renderModeDropdown_, 0); // Default to Adaptive.
    lv_obj_add_event_cb(renderModeDropdown_, onRenderModeChanged, LV_EVENT_VALUE_CHANGED, this);

    // World Size toggle slider.
    auto worldSizeBuilder = LVGLBuilder::toggleSlider(container_)
                                .label("World Size")
                                .range(1, 400)   // Max world size
                                .defaultValue(1) // When off, defaults to 1
                                .value(28)       // Initial value when on
                                .sliderWidth(LV_PCT(85))
                                .valueFormat("%.0f")    // Format as floating point with 0 decimals
                                .valueScale(1.0)        // No scaling needed
                                .initiallyEnabled(true) // Start with toggle on to show current size
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
            }
            else if (lv_obj_check_type(child, &lv_slider_class)) {
                worldSizeSlider_ = child;
                spdlog::debug("CoreControls: Found world size slider");
            }
        }

        if (!worldSizeSwitch_) {
            spdlog::error("CoreControls: Failed to find world size switch in container");
        }
        if (!worldSizeSlider_) {
            spdlog::error("CoreControls: Failed to find world size slider in container");
        }
        else {
            // Add RELEASED event handler to the slider for throttling
            // The VALUE_CHANGED event is already handled by the ToggleSliderBuilder
            lv_obj_add_event_cb(worldSizeSlider_, onWorldSizeChanged, LV_EVENT_RELEASED, this);
        }
    }

    // Scale Factor slider (affects SHARP, SMOOTH, LVGL_DEBUG, and ADAPTIVE modes).
    scaleFactorSlider_ = LVGLBuilder::slider(container_)
                             .size(LV_PCT(90), 10)
                             .range(1, 200) // 0.01 to 2.0, scaled by 100
                             .value(50)     // Default 0.5
                             .label("Render Scale")
                             .valueLabel("%.2f")
                             .valueTransform([](int32_t val) { return val / 100.0; })
                             .callback(onScaleFactorChanged, this)
                             .buildOrLog();

    // Set initial render mode in dropdown.
    setRenderMode(initialMode);

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

void CoreControls::setRenderMode(RenderMode mode)
{
    currentRenderMode_ = mode; // Track the current mode.
    if (!renderModeDropdown_) return;

    // Map RenderMode to dropdown index.
    // Order: "Adaptive\nSharp\nSmooth\nPixel Perfect\nLVGL Debug".
    uint16_t index = 0;
    switch (mode) {
        case RenderMode::ADAPTIVE:
            index = 0;
            break;
        case RenderMode::SHARP:
            index = 1;
            break;
        case RenderMode::SMOOTH:
            index = 2;
            break;
        case RenderMode::PIXEL_PERFECT:
            index = 3;
            break;
        case RenderMode::LVGL_DEBUG:
            index = 4;
            break;
    }

    lv_dropdown_set_selected(renderModeDropdown_, index);
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
    const Api::Reset::Command cmd{};
    self->wsClient_->sendCommand(cmd);
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

void CoreControls::onRenderModeChanged(lv_event_t* e)
{
    CoreControls* self = static_cast<CoreControls*>(lv_event_get_user_data(e));
    if (!self) return;

    lv_obj_t* dropdown = static_cast<lv_obj_t*>(lv_event_get_target(e));
    if (!dropdown) return;

    uint16_t selected = lv_dropdown_get_selected(dropdown);

    // Map dropdown index to RenderMode.
    // Order matches dropdown options: "Adaptive\nSharp\nSmooth\nPixel Perfect\nLVGL Debug".
    RenderMode mode;
    switch (selected) {
        case 0:
            mode = RenderMode::ADAPTIVE;
            break;
        case 1:
            mode = RenderMode::SHARP;
            break;
        case 2:
            mode = RenderMode::SMOOTH;
            break;
        case 3:
            mode = RenderMode::PIXEL_PERFECT;
            break;
        case 4:
            mode = RenderMode::LVGL_DEBUG;
            break;
        default:
            mode = RenderMode::ADAPTIVE;
            break;
    }

    spdlog::info("CoreControls: Render mode changed to {}", renderModeToString(mode));

    // Track the current mode.
    self->currentRenderMode_ = mode;

    // Queue UI-local render mode select event.
    UiApi::RenderModeSelect::Cwc cwc;
    cwc.command.mode = mode;
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
        // Send resize command with size 1x1 (minimum world size).
        const Api::WorldResize::Command cmd{ .width = 1, .height = 1 };
        self->wsClient_->sendCommand(cmd);
        spdlog::info("CoreControls: Resizing world to 1x1 (toggle off)");
    }
    else {
        // Get the current slider value and resize to that.
        if (!self->worldSizeSlider_) {
            spdlog::error("CoreControls: worldSizeSlider_ is null!");
            // Use default value if slider is not available.
            const Api::WorldResize::Command cmd{ .width = 28, .height = 28 };
            self->wsClient_->sendCommand(cmd);
            spdlog::info("CoreControls: Resizing world to 28x28 (default, slider unavailable)");
        }
        else {
            int32_t value = lv_slider_get_value(self->worldSizeSlider_);
            const Api::WorldResize::Command cmd{ .width = static_cast<uint32_t>(value),
                                                 .height = static_cast<uint32_t>(value) };
            self->wsClient_->sendCommand(cmd);
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
    }
    else if (code != LV_EVENT_RELEASED) {
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

        // Send WorldResize API command.
        const Api::WorldResize::Command cmd{ .width = static_cast<uint32_t>(value),
                                             .height = static_cast<uint32_t>(value) };
        self->wsClient_->sendCommand(cmd);
        spdlog::info("CoreControls: Resizing world to {}x{}", value, value);
    }
}

void CoreControls::onScaleFactorChanged(lv_event_t* e)
{
    CoreControls* self = static_cast<CoreControls*>(lv_event_get_user_data(e));
    if (!self) {
        spdlog::error("CoreControls: onScaleFactorChanged called with null self");
        return;
    }

    lv_obj_t* slider = static_cast<lv_obj_t*>(lv_event_get_target(e));
    int32_t value = lv_slider_get_value(slider);

    // Convert from integer (50-300) to double (0.5-3.0).
    double scaleFactor = value / 100.0;

    spdlog::info("CoreControls: Scale factor changed to {:.2f}", scaleFactor);

    // Update the global scale factor.
    setSharpScaleFactor(scaleFactor);

    // Trigger renderer reinitialization by sending RenderModeSelect event.
    // Preserve the current mode (don't force SHARP).
    UiApi::RenderModeSelect::Cwc cwc;
    cwc.command.mode = self->currentRenderMode_;
    cwc.callback = [](auto&&) {}; // No response needed.
    self->eventSink_.queueEvent(cwc);
}

} // namespace Ui
} // namespace DirtSim
