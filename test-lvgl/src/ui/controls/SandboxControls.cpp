#include "SandboxControls.h"
#include "server/api/ScenarioConfigSet.h"
#include "server/api/SeedAdd.h"
#include "server/api/SpawnDirtBall.h"
#include "ui/state-machine/network/WebSocketClient.h"
#include "ui/ui_builders/LVGLBuilder.h"
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

namespace DirtSim {
namespace Ui {

SandboxControls::SandboxControls(
    lv_obj_t* container, WebSocketClient* wsClient, const SandboxConfig& config)
    : container_(container)
    , wsClient_(wsClient)
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

    // Drop Dirt Ball button.
    dropDirtBallButton_ = LVGLBuilder::button(container_)
                              .size(LV_PCT(90), 40)
                              .text("Drop Dirt Ball")
                              .callback(onDropDirtBallClicked, this)
                              .buildOrLog();

    // Rain slider.
    rainSlider_ = LVGLBuilder::slider(container_)
                      .size(LV_PCT(80), 10)
                      .range(0, 100)
                      .value(static_cast<int>(config.rain_rate * 10))
                      .label("Rain Rate")
                      .callback(onRainSliderChanged, this)
                      .buildOrLog();

    spdlog::info("SandboxControls: Initialized");
}

SandboxControls::~SandboxControls()
{
    spdlog::info("SandboxControls: Destroyed");
}

void SandboxControls::updateFromConfig(const SandboxConfig& config)
{
    // Update toggle states (would need to check current state to avoid redundant updates).
    // For now, this is a placeholder - typically called when config changes from server.
}

void SandboxControls::sendConfigUpdate(const ScenarioConfig& config)
{
    if (!wsClient_ || !wsClient_->isConnected())
        return;

    Api::ScenarioConfigSet::Command cmd;
    cmd.config = config;

    nlohmann::json j = cmd.toJson();
    j["command"] = "scenario_config_set";

    spdlog::debug("SandboxControls: Sending config update");
    wsClient_->send(j.dump());
}

void SandboxControls::onAddSeedClicked(lv_event_t* e)
{
    lv_obj_t* target = static_cast<lv_obj_t*>(lv_event_get_target(e));
    SandboxControls* self = static_cast<SandboxControls*>(lv_obj_get_user_data(target));
    if (!self) return;

    spdlog::info("SandboxControls: Add Seed button clicked");

    if (self->wsClient_ && self->wsClient_->isConnected()) {
        Api::SeedAdd::Command cmd;
        cmd.x = self->worldWidth_ / 2;
        cmd.y = 5;

        nlohmann::json json = cmd.toJson();
        json["command"] = "seed_add";

        spdlog::info("SandboxControls: Sending seed_add at ({}, {})", cmd.x, cmd.y);
        self->wsClient_->send(json.dump());
    }
}

void SandboxControls::onDropDirtBallClicked(lv_event_t* e)
{
    lv_obj_t* target = static_cast<lv_obj_t*>(lv_event_get_target(e));
    SandboxControls* self = static_cast<SandboxControls*>(lv_obj_get_user_data(target));
    if (!self) return;

    spdlog::info("SandboxControls: Drop Dirt Ball button clicked");

    if (self->wsClient_ && self->wsClient_->isConnected()) {
        nlohmann::json cmd;
        cmd["command"] = "spawn_dirt_ball";

        spdlog::info("SandboxControls: Sending spawn_dirt_ball command");
        self->wsClient_->send(cmd.dump());
    }
}

void SandboxControls::onQuadrantToggled(lv_event_t* e)
{
    lv_obj_t* target = static_cast<lv_obj_t*>(lv_event_get_target(e));
    SandboxControls* self = static_cast<SandboxControls*>(lv_obj_get_user_data(target));
    if (!self) return;

    bool enabled = lv_obj_has_state(target, LV_STATE_CHECKED);
    spdlog::info("SandboxControls: Quadrant toggled to {}", enabled ? "ON" : "OFF");

    SandboxConfig config;
    config.quadrant_enabled = enabled;
    // TODO: Get other current config values before sending.
    self->sendConfigUpdate(config);
}

void SandboxControls::onWaterColumnToggled(lv_event_t* e)
{
    lv_obj_t* target = static_cast<lv_obj_t*>(lv_event_get_target(e));
    SandboxControls* self = static_cast<SandboxControls*>(lv_obj_get_user_data(target));
    if (!self) return;

    bool enabled = lv_obj_has_state(target, LV_STATE_CHECKED);
    spdlog::info("SandboxControls: Water Column toggled to {}", enabled ? "ON" : "OFF");

    SandboxConfig config;
    config.water_column_enabled = enabled;
    // TODO: Get other current config values before sending.
    self->sendConfigUpdate(config);
}

void SandboxControls::onRightThrowToggled(lv_event_t* e)
{
    lv_obj_t* target = static_cast<lv_obj_t*>(lv_event_get_target(e));
    SandboxControls* self = static_cast<SandboxControls*>(lv_obj_get_user_data(target));
    if (!self) return;

    bool enabled = lv_obj_has_state(target, LV_STATE_CHECKED);
    spdlog::info("SandboxControls: Right Throw toggled to {}", enabled ? "ON" : "OFF");

    SandboxConfig config;
    config.right_throw_enabled = enabled;
    // TODO: Get other current config values before sending.
    self->sendConfigUpdate(config);
}

void SandboxControls::onRainSliderChanged(lv_event_t* e)
{
    lv_obj_t* target = static_cast<lv_obj_t*>(lv_event_get_target(e));
    SandboxControls* self = static_cast<SandboxControls*>(lv_obj_get_user_data(target));
    if (!self) return;

    int value = lv_slider_get_value(target);
    double rainRate = value / 10.0;

    spdlog::info("SandboxControls: Rain rate changed to {:.1f}", rainRate);

    SandboxConfig config;
    config.rain_rate = rainRate;
    // TODO: Get other current config values before sending.
    self->sendConfigUpdate(config);
}

} // namespace Ui
} // namespace DirtSim
