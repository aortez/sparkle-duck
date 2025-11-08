#include "ControlPanel.h"
#include "ui/state-machine/EventSink.h"
#include "ui/state-machine/network/WebSocketClient.h"
#include "ui/ui_builders/LVGLBuilder.h"
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

namespace DirtSim {
namespace Ui {

ControlPanel::ControlPanel(lv_obj_t* container, WebSocketClient* wsClient, EventSink& eventSink)
    : container_(container), wsClient_(wsClient), eventSink_(eventSink)
{
    if (!container_) {
        spdlog::error("ControlPanel: Null container provided");
        return;
    }

    // Create left-side panel container for controls.
    panelContainer_ = lv_obj_create(container_);
    lv_obj_set_size(panelContainer_, 260, LV_PCT(100));  // 260px wide (30% wider), full height.
    lv_obj_align(panelContainer_, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_set_flex_flow(panelContainer_, LV_FLEX_FLOW_COLUMN);  // Stack controls vertically.
    lv_obj_set_flex_align(panelContainer_, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // Reduce padding/gaps to fit more controls without scrolling.
    lv_obj_set_style_pad_row(panelContainer_, 2, 0);  // Minimal spacing between items.
    lv_obj_set_style_pad_all(panelContainer_, 5, 0);  // Minimal padding around edges.
    lv_obj_set_scroll_dir(panelContainer_, LV_DIR_VER);  // Enable vertical scrolling.
    lv_obj_set_scrollbar_mode(panelContainer_, LV_SCROLLBAR_MODE_AUTO);  // Show scrollbar when needed.

    // Create core controls.
    createCoreControls();

    spdlog::info("ControlPanel: Initialized with core controls");
}

ControlPanel::~ControlPanel()
{
    // LVGL widgets are automatically cleaned up when parent is deleted.
    spdlog::info("ControlPanel: Destroyed");
}

void ControlPanel::updateFromWorldData(const WorldData& data)
{
    // Rebuild scenario controls if scenario changed.
    if (data.scenario_id != currentScenarioId_) {
        spdlog::info("ControlPanel: Scenario changed to '{}'", data.scenario_id);
        clearScenarioControls();
        createScenarioControls(data.scenario_id, data.scenario_config);
        currentScenarioId_ = data.scenario_id;
    }
}

void ControlPanel::createCoreControls()
{
    // Quit button.
    quitButton_ = lv_btn_create(panelContainer_);
    lv_obj_set_width(quitButton_, LV_PCT(90));
    lv_obj_t* quitLabel = lv_label_create(quitButton_);
    lv_label_set_text(quitLabel, "Quit");
    lv_obj_center(quitLabel);
    lv_obj_set_user_data(quitButton_, this);
    lv_obj_add_event_cb(quitButton_, onQuitClicked, LV_EVENT_CLICKED, nullptr);

    // Debug toggle.
    debugSwitch_ = lv_switch_create(panelContainer_);
    lv_obj_set_user_data(debugSwitch_, this);
    lv_obj_add_event_cb(debugSwitch_, onDebugToggled, LV_EVENT_VALUE_CHANGED, nullptr);

    lv_obj_t* debugLabel = lv_label_create(panelContainer_);
    lv_label_set_text(debugLabel, "Debug Draw");

    spdlog::debug("ControlPanel: Core controls created");
}

void ControlPanel::createScenarioControls(const std::string& scenarioId, const ScenarioConfig& config)
{
    // Create scenario panel container.
    scenarioPanel_ = lv_obj_create(panelContainer_);
    lv_obj_set_size(scenarioPanel_, LV_PCT(100), LV_SIZE_CONTENT);  // Full width, height fits content.
    lv_obj_set_flex_flow(scenarioPanel_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(scenarioPanel_, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // Create controls based on scenario type.
    if (scenarioId == "sandbox" && std::holds_alternative<SandboxConfig>(config)) {
        createSandboxControls(std::get<SandboxConfig>(config));
    }
    // TODO: Add other scenario control creators here.

    spdlog::debug("ControlPanel: Scenario controls created for '{}'", scenarioId);
}

void ControlPanel::clearScenarioControls()
{
    if (scenarioPanel_) {
        lv_obj_del(scenarioPanel_);
        scenarioPanel_ = nullptr;
        sandboxQuadrantSwitch_ = nullptr;
        sandboxWaterColumnSwitch_ = nullptr;
        sandboxRightThrowSwitch_ = nullptr;
        sandboxTopDropSwitch_ = nullptr;
        sandboxRainSlider_ = nullptr;
    }
}

void ControlPanel::createSandboxControls(const SandboxConfig& config)
{
    // Scenario label.
    lv_obj_t* scenarioLabel = lv_label_create(scenarioPanel_);
    lv_label_set_text(scenarioLabel, "--- Sandbox ---");

    // Quadrant toggle.
    sandboxQuadrantSwitch_ = LVGLBuilder::labeledSwitch(scenarioPanel_)
        .label("Quadrant")
        .initialState(config.quadrant_enabled)
        .callback(onSandboxQuadrantToggled, this)
        .buildOrLog();

    // Water column toggle.
    sandboxWaterColumnSwitch_ = LVGLBuilder::labeledSwitch(scenarioPanel_)
        .label("Water Column")
        .initialState(config.water_column_enabled)
        .callback(onSandboxWaterColumnToggled, this)
        .buildOrLog();

    // Right throw toggle.
    sandboxRightThrowSwitch_ = LVGLBuilder::labeledSwitch(scenarioPanel_)
        .label("Right Throw")
        .initialState(config.right_throw_enabled)
        .callback(onSandboxRightThrowToggled, this)
        .buildOrLog();

    // Top drop toggle.
    sandboxTopDropSwitch_ = LVGLBuilder::labeledSwitch(scenarioPanel_)
        .label("Top Drop")
        .initialState(config.top_drop_enabled)
        .callback(onSandboxTopDropToggled, this)
        .buildOrLog();

    // Rain slider.
    sandboxRainSlider_ = LVGLBuilder::slider(scenarioPanel_)
        .size(LV_PCT(80), 10)
        .range(0, 100)
        .value(static_cast<int>(config.rain_rate * 10))
        .label("Rain Rate")
        .callback(onSandboxRainSliderChanged, this)
        .buildOrLog();

    spdlog::debug("ControlPanel: Sandbox controls created");
}

// ============================================================================
// Event Handlers
// ============================================================================

void ControlPanel::onQuitClicked(lv_event_t* e)
{
    auto* panel = static_cast<ControlPanel*>(lv_obj_get_user_data(static_cast<lv_obj_t*>(lv_event_get_target(e))));
    if (!panel || !panel->wsClient_) return;

    spdlog::info("ControlPanel: Quit button clicked");

    // Send exit command to DSSM.
    nlohmann::json cmd = {{"command", "exit"}};
    panel->wsClient_->send(cmd.dump());
}

void ControlPanel::onDebugToggled(lv_event_t* e)
{
    auto* panel = static_cast<ControlPanel*>(lv_obj_get_user_data(static_cast<lv_obj_t*>(lv_event_get_target(e))));
    if (!panel) return;

    bool enabled = lv_obj_has_state(static_cast<lv_obj_t*>(lv_event_get_target(e)), LV_STATE_CHECKED);
    spdlog::info("ControlPanel: Debug draw toggled: {}", enabled);

    panel->sendDebugUpdate(enabled);
}

void ControlPanel::onSandboxQuadrantToggled(lv_event_t* e)
{
    auto* panel = static_cast<ControlPanel*>(lv_obj_get_user_data(static_cast<lv_obj_t*>(lv_event_get_target(e))));
    if (!panel) return;

    bool enabled = lv_obj_has_state(static_cast<lv_obj_t*>(lv_event_get_target(e)), LV_STATE_CHECKED);
    spdlog::info("ControlPanel: Sandbox quadrant toggled: {}", enabled);

    // Create updated config.
    SandboxConfig config;
    config.quadrant_enabled = enabled;
    config.water_column_enabled = panel->sandboxWaterColumnSwitch_
        ? lv_obj_has_state(static_cast<lv_obj_t*>(panel->sandboxWaterColumnSwitch_), LV_STATE_CHECKED) : true;
    config.right_throw_enabled = panel->sandboxRightThrowSwitch_
        ? lv_obj_has_state(static_cast<lv_obj_t*>(panel->sandboxRightThrowSwitch_), LV_STATE_CHECKED) : true;
    config.top_drop_enabled = panel->sandboxTopDropSwitch_
        ? lv_obj_has_state(static_cast<lv_obj_t*>(panel->sandboxTopDropSwitch_), LV_STATE_CHECKED) : true;
    config.rain_rate = panel->sandboxRainSlider_
        ? lv_slider_get_value(static_cast<lv_obj_t*>(panel->sandboxRainSlider_)) / 10.0 : 0.0;

    panel->sendConfigUpdate(config);
}

void ControlPanel::onSandboxWaterColumnToggled(lv_event_t* e)
{
    auto* panel = static_cast<ControlPanel*>(lv_obj_get_user_data(static_cast<lv_obj_t*>(lv_event_get_target(e))));
    if (!panel) return;

    bool enabled = lv_obj_has_state(static_cast<lv_obj_t*>(lv_event_get_target(e)), LV_STATE_CHECKED);
    spdlog::info("ControlPanel: Sandbox water column toggled: {}", enabled);

    // Create updated config with all current values.
    SandboxConfig config;
    config.quadrant_enabled = panel->sandboxQuadrantSwitch_
        ? lv_obj_has_state(static_cast<lv_obj_t*>(panel->sandboxQuadrantSwitch_), LV_STATE_CHECKED) : true;
    config.water_column_enabled = enabled;
    config.right_throw_enabled = panel->sandboxRightThrowSwitch_
        ? lv_obj_has_state(static_cast<lv_obj_t*>(panel->sandboxRightThrowSwitch_), LV_STATE_CHECKED) : true;
    config.top_drop_enabled = panel->sandboxTopDropSwitch_
        ? lv_obj_has_state(static_cast<lv_obj_t*>(panel->sandboxTopDropSwitch_), LV_STATE_CHECKED) : true;
    config.rain_rate = panel->sandboxRainSlider_
        ? lv_slider_get_value(static_cast<lv_obj_t*>(panel->sandboxRainSlider_)) / 10.0 : 0.0;

    panel->sendConfigUpdate(config);
}

void ControlPanel::onSandboxRightThrowToggled(lv_event_t* e)
{
    auto* panel = static_cast<ControlPanel*>(lv_obj_get_user_data(static_cast<lv_obj_t*>(lv_event_get_target(e))));
    if (!panel) return;

    bool enabled = lv_obj_has_state(static_cast<lv_obj_t*>(lv_event_get_target(e)), LV_STATE_CHECKED);
    spdlog::info("ControlPanel: Sandbox right throw toggled: {}", enabled);

    SandboxConfig config;
    config.quadrant_enabled = panel->sandboxQuadrantSwitch_
        ? lv_obj_has_state(static_cast<lv_obj_t*>(panel->sandboxQuadrantSwitch_), LV_STATE_CHECKED) : true;
    config.water_column_enabled = panel->sandboxWaterColumnSwitch_
        ? lv_obj_has_state(static_cast<lv_obj_t*>(panel->sandboxWaterColumnSwitch_), LV_STATE_CHECKED) : true;
    config.right_throw_enabled = enabled;
    config.top_drop_enabled = panel->sandboxTopDropSwitch_
        ? lv_obj_has_state(static_cast<lv_obj_t*>(panel->sandboxTopDropSwitch_), LV_STATE_CHECKED) : true;
    config.rain_rate = panel->sandboxRainSlider_
        ? lv_slider_get_value(static_cast<lv_obj_t*>(panel->sandboxRainSlider_)) / 10.0 : 0.0;

    panel->sendConfigUpdate(config);
}

void ControlPanel::onSandboxTopDropToggled(lv_event_t* e)
{
    auto* panel = static_cast<ControlPanel*>(lv_obj_get_user_data(static_cast<lv_obj_t*>(lv_event_get_target(e))));
    if (!panel) return;

    bool enabled = lv_obj_has_state(static_cast<lv_obj_t*>(lv_event_get_target(e)), LV_STATE_CHECKED);
    spdlog::info("ControlPanel: Sandbox top drop toggled: {}", enabled);

    SandboxConfig config;
    config.quadrant_enabled = panel->sandboxQuadrantSwitch_
        ? lv_obj_has_state(static_cast<lv_obj_t*>(panel->sandboxQuadrantSwitch_), LV_STATE_CHECKED) : true;
    config.water_column_enabled = panel->sandboxWaterColumnSwitch_
        ? lv_obj_has_state(static_cast<lv_obj_t*>(panel->sandboxWaterColumnSwitch_), LV_STATE_CHECKED) : true;
    config.right_throw_enabled = panel->sandboxRightThrowSwitch_
        ? lv_obj_has_state(static_cast<lv_obj_t*>(panel->sandboxRightThrowSwitch_), LV_STATE_CHECKED) : true;
    config.top_drop_enabled = enabled;
    config.rain_rate = panel->sandboxRainSlider_
        ? lv_slider_get_value(static_cast<lv_obj_t*>(panel->sandboxRainSlider_)) / 10.0 : 0.0;

    panel->sendConfigUpdate(config);
}

void ControlPanel::onSandboxRainSliderChanged(lv_event_t* e)
{
    auto* panel = static_cast<ControlPanel*>(lv_obj_get_user_data(static_cast<lv_obj_t*>(lv_event_get_target(e))));
    if (!panel) return;

    lv_obj_t* slider = static_cast<lv_obj_t*>(lv_event_get_target(e));
    int32_t sliderValue = lv_slider_get_value(slider);
    double rainRate = sliderValue / 10.0;
    spdlog::info("ControlPanel: Sandbox rain rate changed: {}", rainRate);

    SandboxConfig config;
    config.quadrant_enabled = panel->sandboxQuadrantSwitch_
        ? lv_obj_has_state(static_cast<lv_obj_t*>(panel->sandboxQuadrantSwitch_), LV_STATE_CHECKED) : true;
    config.water_column_enabled = panel->sandboxWaterColumnSwitch_
        ? lv_obj_has_state(static_cast<lv_obj_t*>(panel->sandboxWaterColumnSwitch_), LV_STATE_CHECKED) : true;
    config.right_throw_enabled = panel->sandboxRightThrowSwitch_
        ? lv_obj_has_state(static_cast<lv_obj_t*>(panel->sandboxRightThrowSwitch_), LV_STATE_CHECKED) : true;
    config.top_drop_enabled = panel->sandboxTopDropSwitch_
        ? lv_obj_has_state(static_cast<lv_obj_t*>(panel->sandboxTopDropSwitch_), LV_STATE_CHECKED) : true;
    config.rain_rate = rainRate;

    panel->sendConfigUpdate(config);
}

// ============================================================================
// Command Sending
// ============================================================================

void ControlPanel::sendConfigUpdate(const ScenarioConfig& config)
{
    if (!wsClient_ || !wsClient_->isConnected()) {
        spdlog::warn("ControlPanel: Cannot send config update, not connected to DSSM");
        return;
    }

    // Create command JSON.
    nlohmann::json cmd;
    cmd["command"] = "scenario_config_set";
    cmd["config"] = config;  // Uses ADL to_json from ScenarioConfig.h.

    // Send to DSSM.
    wsClient_->send(cmd.dump());
    spdlog::debug("ControlPanel: Sent scenario config update to DSSM");
}

void ControlPanel::sendDebugUpdate(bool enabled)
{
    // Queue DrawDebugToggle command to UI state machine (UI-local, not sent to server).
    auto cmd = UiApi::DrawDebugToggle::Command{enabled};
    auto cwc = UiApi::DrawDebugToggle::Cwc(std::move(cmd), [](const auto& /*response*/) {
        // No action needed on response.
    });

    eventSink_.queueEvent(std::move(cwc));
    spdlog::info("ControlPanel: Queued DrawDebugToggle command (enabled: {})", enabled);
}

} // namespace Ui
} // namespace DirtSim
