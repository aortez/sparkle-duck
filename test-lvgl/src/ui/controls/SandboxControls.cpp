#include "SandboxControls.h"
#include "server/api/ScenarioConfigSet.h"
#include "server/api/SeedAdd.h"
#include "server/api/SpawnDirtBall.h"
#include "ui/state-machine/network/WebSocketClient.h"
#include "ui/ui_builders/LVGLBuilder.h"
#include <chrono>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

namespace DirtSim {
namespace Ui {

SandboxControls::SandboxControls(
    lv_obj_t* container, WebSocketClient* wsClient, const SandboxConfig& config)
    : container_(container), wsClient_(wsClient)
{
    // Scenario label.
    lv_obj_t* scenarioLabel = lv_label_create(container_);
    lv_label_set_text(scenarioLabel, "--- Sandbox ---");

    // Add Seed button.
    addSeedButton_ = LVGLBuilder::button(container_)
                         .size(LV_PCT(90), 40)
                         .text("Add Seed")
                         .callback(onAddSeedClicked, this)
                         .buildOrLog();

    // Drop Dirt Ball button.
    dropDirtBallButton_ = LVGLBuilder::button(container_)
                              .size(LV_PCT(90), 40)
                              .text("Drop Dirt Ball")
                              .callback(onDropDirtBallClicked, this)
                              .buildOrLog();

    // Quadrant toggle.
    quadrantSwitch_ = LVGLBuilder::labeledSwitch(container_)
                          .label("Quadrant")
                          .initialState(config.quadrant_enabled)
                          .callback(onQuadrantToggled, this)
                          .buildOrLog();

    // Water column toggle.
    waterColumnSwitch_ = LVGLBuilder::labeledSwitch(container_)
                             .label("Water Column")
                             .initialState(config.water_column_enabled)
                             .callback(onWaterColumnToggled, this)
                             .buildOrLog();

    // Right throw toggle.
    rightThrowSwitch_ = LVGLBuilder::labeledSwitch(container_)
                            .label("Right Throw")
                            .initialState(config.right_throw_enabled)
                            .callback(onRightThrowToggled, this)
                            .buildOrLog();

    // Rain slider - Don't use .label() to avoid double callbacks
    lv_obj_t* rainLabel = lv_label_create(container_);
    lv_label_set_text(rainLabel, "Rain Rate");

    rainSlider_ = LVGLBuilder::slider(container_)
                      .size(LV_PCT(80), 10)
                      .range(0, 100)
                      .value(static_cast<int>(config.rain_rate * 10))
                      .callback(onRainSliderChanged, this)
                      .buildOrLog();

    // Initialization complete - allow callbacks to send updates now
    initializing_ = false;

    spdlog::info("SandboxControls: Initialized");
}

SandboxControls::~SandboxControls()
{
    spdlog::info("SandboxControls: Destroyed");
}

void SandboxControls::updateFromConfig([[maybe_unused]] const SandboxConfig& config)
{
    // Update toggle states (would need to check current state to avoid redundant updates).
    // For now, this is a placeholder - typically called when config changes from server.
}

SandboxConfig SandboxControls::getCurrentConfig() const
{
    SandboxConfig config;

    // Get current state of all controls
    if (quadrantSwitch_) {
        config.quadrant_enabled = lv_obj_has_state(quadrantSwitch_, LV_STATE_CHECKED);
    }

    if (waterColumnSwitch_) {
        config.water_column_enabled = lv_obj_has_state(waterColumnSwitch_, LV_STATE_CHECKED);
    }

    if (rightThrowSwitch_) {
        config.right_throw_enabled = lv_obj_has_state(rightThrowSwitch_, LV_STATE_CHECKED);
    }

    if (rainSlider_) {
        int value = lv_slider_get_value(rainSlider_);
        config.rain_rate = value / 10.0;
    }

    return config;
}

void SandboxControls::sendConfigUpdate(const ScenarioConfig& config)
{
    if (!wsClient_ || !wsClient_->isConnected()) return;

    // Track rapid calls
    static int updateCount = 0;
    static auto lastUpdateTime = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();
    auto elapsed =
        std::chrono::duration_cast<std::chrono::milliseconds>(now - lastUpdateTime).count();

    if (elapsed < 1000) { // Within 1 second
        updateCount++;
        if (updateCount > 5) {
            spdlog::error(
                "SandboxControls: LOOP DETECTED! {} config updates in {}ms", updateCount, elapsed);
            // Don't send if we're in a loop
            return;
        }
    }
    else {
        updateCount = 1;
    }
    lastUpdateTime = now;

    const Api::ScenarioConfigSet::Command cmd{ .config = config };

    spdlog::info(
        "SandboxControls: Sending config update (update #{} in {}ms)", updateCount, elapsed);
    wsClient_->sendCommand(cmd);
}

void SandboxControls::onAddSeedClicked(lv_event_t* e)
{
    SandboxControls* self = static_cast<SandboxControls*>(lv_event_get_user_data(e));
    if (!self) {
        spdlog::error("SandboxControls: onAddSeedClicked called with null self");
        return;
    }

    spdlog::info("SandboxControls: Add Seed button clicked");

    if (self->wsClient_ && self->wsClient_->isConnected()) {
        const Api::SeedAdd::Command cmd{ .x = static_cast<int>(self->worldWidth_ / 2), .y = 5 };

        spdlog::info("SandboxControls: Sending seed_add at ({}, {})", cmd.x, cmd.y);
        self->wsClient_->sendCommand(cmd);
    }
}

void SandboxControls::onDropDirtBallClicked(lv_event_t* e)
{
    SandboxControls* self = static_cast<SandboxControls*>(lv_event_get_user_data(e));
    if (!self) {
        spdlog::error("SandboxControls: onDropDirtBallClicked called with null self");
        return;
    }

    spdlog::info("SandboxControls: Drop Dirt Ball button clicked");

    if (self->wsClient_ && self->wsClient_->isConnected()) {
        const Api::SpawnDirtBall::Command cmd{};

        spdlog::info("SandboxControls: Sending spawn_dirt_ball command");
        self->wsClient_->sendCommand(cmd);
    }
}

void SandboxControls::onQuadrantToggled(lv_event_t* e)
{
    SandboxControls* self = static_cast<SandboxControls*>(lv_event_get_user_data(e));
    if (!self) {
        spdlog::error("SandboxControls: onQuadrantToggled called with null self");
        return;
    }

    // Don't send updates during initialization
    if (self->initializing_) {
        spdlog::debug("SandboxControls: Ignoring quadrant toggle during initialization");
        return;
    }

    lv_obj_t* target = static_cast<lv_obj_t*>(lv_event_get_target(e));
    bool enabled = lv_obj_has_state(target, LV_STATE_CHECKED);
    spdlog::info("SandboxControls: Quadrant toggled to {}", enabled ? "ON" : "OFF");

    // Get complete current config from all controls
    SandboxConfig config = self->getCurrentConfig();
    self->sendConfigUpdate(config);
}

void SandboxControls::onWaterColumnToggled(lv_event_t* e)
{
    SandboxControls* self = static_cast<SandboxControls*>(lv_event_get_user_data(e));
    if (!self) {
        spdlog::error("SandboxControls: onWaterColumnToggled called with null self");
        return;
    }

    // Don't send updates during initialization
    if (self->initializing_) {
        spdlog::debug("SandboxControls: Ignoring water column toggle during initialization");
        return;
    }

    lv_obj_t* target = static_cast<lv_obj_t*>(lv_event_get_target(e));
    bool enabled = lv_obj_has_state(target, LV_STATE_CHECKED);
    spdlog::info("SandboxControls: Water Column toggled to {}", enabled ? "ON" : "OFF");

    // Get complete current config from all controls
    SandboxConfig config = self->getCurrentConfig();
    self->sendConfigUpdate(config);
}

void SandboxControls::onRightThrowToggled(lv_event_t* e)
{
    SandboxControls* self = static_cast<SandboxControls*>(lv_event_get_user_data(e));
    if (!self) {
        spdlog::error("SandboxControls: onRightThrowToggled called with null self");
        return;
    }

    // Don't send updates during initialization
    if (self->initializing_) {
        spdlog::debug("SandboxControls: Ignoring right throw toggle during initialization");
        return;
    }

    lv_obj_t* target = static_cast<lv_obj_t*>(lv_event_get_target(e));
    bool enabled = lv_obj_has_state(target, LV_STATE_CHECKED);
    spdlog::info("SandboxControls: Right Throw toggled to {}", enabled ? "ON" : "OFF");

    // Get complete current config from all controls
    SandboxConfig config = self->getCurrentConfig();
    self->sendConfigUpdate(config);
}

void SandboxControls::onRainSliderChanged(lv_event_t* e)
{
    // User data is passed through the event, not stored on the slider object
    SandboxControls* self = static_cast<SandboxControls*>(lv_event_get_user_data(e));
    if (!self) {
        spdlog::error("SandboxControls: onRainSliderChanged called with null self");
        return;
    }

    // Don't send updates during initialization
    if (self->initializing_) {
        spdlog::debug("SandboxControls: Ignoring rain slider during initialization");
        return;
    }

    lv_obj_t* target = static_cast<lv_obj_t*>(lv_event_get_target(e));
    int value = lv_slider_get_value(target);
    double rainRate = value / 10.0;

    // Track last value to prevent redundant updates
    static double lastRainRate = -1.0;
    if (std::abs(rainRate - lastRainRate) < 0.01) {
        // Value hasn't changed - don't send update
        // This prevents infinite loops from spurious VALUE_CHANGED events
        return;
    }
    lastRainRate = rainRate;

    spdlog::info("SandboxControls: Rain rate changed to {:.1f}", rainRate);

    // Get complete current config from all controls
    SandboxConfig config = self->getCurrentConfig();
    self->sendConfigUpdate(config);
}

} // namespace Ui
} // namespace DirtSim
