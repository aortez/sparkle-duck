#include "PhysicsControls.h"
#include "ui/state-machine/network/WebSocketClient.h"
#include "ui/ui_builders/LVGLBuilder.h"
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

namespace DirtSim {
namespace Ui {

PhysicsControls::PhysicsControls(lv_obj_t* container, WebSocketClient* wsClient)
    : container_(container)
    , wsClient_(wsClient)
{
    // Create 3-column layout.
    lv_obj_set_flex_flow(container_, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(
        container_, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

    // Column 1: General Physics.
    column1_ = lv_obj_create(container_);
    lv_obj_set_size(column1_, LV_PCT(30), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(column1_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(column1_, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(column1_, 4, 0);
    lv_obj_set_style_pad_all(column1_, 8, 0);

    lv_obj_t* label1 = lv_label_create(column1_);
    lv_label_set_text(label1, "General Physics");
    lv_obj_set_style_text_font(label1, &lv_font_montserrat_14, 0);

    timescaleControl_ = LVGLBuilder::toggleSlider(column1_)
                            .label("Timescale")
                            .range(0, 200)
                            .value(100)
                            .defaultValue(100)
                            .valueScale(0.01)
                            .valueFormat("%.2fx")
                            .initiallyEnabled(false)
                            .sliderWidth(180)
                            .onToggle(onTimescaleToggled, this)
                            .onSliderChange(onTimescaleChanged, this)
                            .buildOrLog();

    gravityControl_ = LVGLBuilder::toggleSlider(column1_)
                          .label("Gravity")
                          .range(0, 200)
                          .value(50)
                          .defaultValue(50)
                          .valueScale(0.01)
                          .valueFormat("%.2f")
                          .initiallyEnabled(false)
                          .sliderWidth(180)
                          .onToggle(onGravityToggled, this)
                          .onSliderChange(onGravityChanged, this)
                          .buildOrLog();

    elasticityControl_ = LVGLBuilder::toggleSlider(column1_)
                             .label("Elasticity")
                             .range(0, 100)
                             .value(80)
                             .defaultValue(80)
                             .valueScale(0.01)
                             .valueFormat("%.2f")
                             .initiallyEnabled(false)
                             .sliderWidth(180)
                             .onToggle(onElasticityToggled, this)
                             .onSliderChange(onElasticityChanged, this)
                             .buildOrLog();

    airResistanceControl_ = LVGLBuilder::toggleSlider(column1_)
                                .label("Air Resistance")
                                .range(0, 100)
                                .value(10)
                                .defaultValue(10)
                                .valueScale(0.01)
                                .valueFormat("%.2f")
                                .initiallyEnabled(false)
                                .sliderWidth(180)
                                .onToggle(onAirResistanceToggled, this)
                                .onSliderChange(onAirResistanceChanged, this)
                                .buildOrLog();

    // Column 2: Pressure.
    column2_ = lv_obj_create(container_);
    lv_obj_set_size(column2_, LV_PCT(30), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(column2_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(column2_, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(column2_, 4, 0);
    lv_obj_set_style_pad_all(column2_, 8, 0);

    lv_obj_t* label2 = lv_label_create(column2_);
    lv_label_set_text(label2, "Pressure");
    lv_obj_set_style_text_font(label2, &lv_font_montserrat_14, 0);

    hydrostaticPressureControl_ = LVGLBuilder::toggleSlider(column2_)
                                      .label("Hydrostatic")
                                      .range(0, 300)
                                      .value(100)
                                      .defaultValue(100)
                                      .valueScale(0.01)
                                      .valueFormat("%.2f")
                                      .initiallyEnabled(true)
                                      .sliderWidth(180)
                                      .onToggle(onHydrostaticPressureToggled, this)
                                      .onSliderChange(onHydrostaticPressureChanged, this)
                                      .buildOrLog();

    dynamicPressureControl_ = LVGLBuilder::toggleSlider(column2_)
                                  .label("Dynamic")
                                  .range(0, 300)
                                  .value(100)
                                  .defaultValue(100)
                                  .valueScale(0.01)
                                  .valueFormat("%.2f")
                                  .initiallyEnabled(true)
                                  .sliderWidth(180)
                                  .onToggle(onDynamicPressureToggled, this)
                                  .onSliderChange(onDynamicPressureChanged, this)
                                  .buildOrLog();

    pressureDiffusionControl_ = LVGLBuilder::toggleSlider(column2_)
                                    .label("Diffusion")
                                    .range(0, 100)
                                    .value(50)
                                    .defaultValue(50)
                                    .valueScale(0.01)
                                    .valueFormat("%.2f")
                                    .initiallyEnabled(true)
                                    .sliderWidth(180)
                                    .onToggle(onPressureDiffusionToggled, this)
                                    .onSliderChange(onPressureDiffusionChanged, this)
                                    .buildOrLog();

    // Column 3: Forces.
    column3_ = lv_obj_create(container_);
    lv_obj_set_size(column3_, LV_PCT(30), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(column3_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(column3_, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(column3_, 4, 0);
    lv_obj_set_style_pad_all(column3_, 8, 0);

    lv_obj_t* label3 = lv_label_create(column3_);
    lv_label_set_text(label3, "Forces");
    lv_obj_set_style_text_font(label3, &lv_font_montserrat_14, 0);

    cohesionForceControl_ = LVGLBuilder::toggleSlider(column3_)
                                .label("Cohesion")
                                .range(0, 30000)
                                .value(15000)
                                .defaultValue(15000)
                                .valueScale(0.01)
                                .valueFormat("%.1f")
                                .initiallyEnabled(true)
                                .sliderWidth(180)
                                .onToggle(onCohesionForceToggled, this)
                                .onSliderChange(onCohesionForceChanged, this)
                                .buildOrLog();

    adhesionControl_ = LVGLBuilder::toggleSlider(column3_)
                           .label("Adhesion")
                           .range(0, 1000)
                           .value(500)
                           .defaultValue(500)
                           .valueScale(0.01)
                           .valueFormat("%.1f")
                           .initiallyEnabled(true)
                           .sliderWidth(180)
                           .onToggle(onAdhesionToggled, this)
                           .onSliderChange(onAdhesionChanged, this)
                           .buildOrLog();

    viscosityControl_ = LVGLBuilder::toggleSlider(column3_)
                            .label("Viscosity")
                            .range(0, 200)
                            .value(100)
                            .defaultValue(100)
                            .valueScale(0.01)
                            .valueFormat("%.2f")
                            .initiallyEnabled(true)
                            .sliderWidth(180)
                            .onToggle(onViscosityToggled, this)
                            .onSliderChange(onViscosityChanged, this)
                            .buildOrLog();

    frictionControl_ = LVGLBuilder::toggleSlider(column3_)
                           .label("Friction")
                           .range(0, 200)
                           .value(100)
                           .defaultValue(100)
                           .valueScale(0.01)
                           .valueFormat("%.2f")
                           .initiallyEnabled(true)
                           .sliderWidth(180)
                           .onToggle(onFrictionToggled, this)
                           .onSliderChange(onFrictionChanged, this)
                           .buildOrLog();

    spdlog::info("PhysicsControls: Initialized");
}

PhysicsControls::~PhysicsControls()
{
    spdlog::info("PhysicsControls: Destroyed");
}

void PhysicsControls::sendPhysicsCommand(const char* commandName, double value)
{
    if (!wsClient_ || !wsClient_->isConnected()) {
        spdlog::warn("PhysicsControls: Cannot send command '{}' - not connected", commandName);
        return;
    }

    nlohmann::json cmd;
    cmd["command"] = commandName;
    cmd["value"] = value;

    spdlog::debug("PhysicsControls: Sending {} = {:.2f}", commandName, value);
    wsClient_->send(cmd.dump());
}

// Column 1: General Physics event handlers.
void PhysicsControls::onTimescaleToggled(lv_event_t* e)
{
    lv_obj_t* target = static_cast<lv_obj_t*>(lv_event_get_target(e));
    PhysicsControls* self = static_cast<PhysicsControls*>(lv_obj_get_user_data(target));
    if (!self) return;

    bool enabled = lv_obj_has_state(target, LV_STATE_CHECKED);
    spdlog::info("PhysicsControls: Timescale toggled to {}", enabled ? "ON" : "OFF");
}

void PhysicsControls::onTimescaleChanged(lv_event_t* e)
{
    lv_obj_t* target = static_cast<lv_obj_t*>(lv_event_get_target(e));
    PhysicsControls* self = static_cast<PhysicsControls*>(lv_obj_get_user_data(target));
    if (!self) return;

    int value = lv_slider_get_value(target);
    double scaledValue = value * 0.01;
    self->sendPhysicsCommand("timescale_set", scaledValue);
}

void PhysicsControls::onGravityToggled(lv_event_t* e)
{
    lv_obj_t* target = static_cast<lv_obj_t*>(lv_event_get_target(e));
    PhysicsControls* self = static_cast<PhysicsControls*>(lv_obj_get_user_data(target));
    if (!self) return;

    bool enabled = lv_obj_has_state(target, LV_STATE_CHECKED);
    spdlog::info("PhysicsControls: Gravity toggled to {}", enabled ? "ON" : "OFF");
}

void PhysicsControls::onGravityChanged(lv_event_t* e)
{
    lv_obj_t* target = static_cast<lv_obj_t*>(lv_event_get_target(e));
    PhysicsControls* self = static_cast<PhysicsControls*>(lv_obj_get_user_data(target));
    if (!self) return;

    int value = lv_slider_get_value(target);
    double scaledValue = value * 0.01;
    self->sendPhysicsCommand("gravity_set", scaledValue);
}

void PhysicsControls::onElasticityToggled(lv_event_t* e)
{
    lv_obj_t* target = static_cast<lv_obj_t*>(lv_event_get_target(e));
    PhysicsControls* self = static_cast<PhysicsControls*>(lv_obj_get_user_data(target));
    if (!self) return;

    bool enabled = lv_obj_has_state(target, LV_STATE_CHECKED);
    spdlog::info("PhysicsControls: Elasticity toggled to {}", enabled ? "ON" : "OFF");
}

void PhysicsControls::onElasticityChanged(lv_event_t* e)
{
    lv_obj_t* target = static_cast<lv_obj_t*>(lv_event_get_target(e));
    PhysicsControls* self = static_cast<PhysicsControls*>(lv_obj_get_user_data(target));
    if (!self) return;

    int value = lv_slider_get_value(target);
    double scaledValue = value * 0.01;
    self->sendPhysicsCommand("elasticity_set", scaledValue);
}

void PhysicsControls::onAirResistanceToggled(lv_event_t* e)
{
    lv_obj_t* target = static_cast<lv_obj_t*>(lv_event_get_target(e));
    PhysicsControls* self = static_cast<PhysicsControls*>(lv_obj_get_user_data(target));
    if (!self) return;

    bool enabled = lv_obj_has_state(target, LV_STATE_CHECKED);
    spdlog::info("PhysicsControls: Air Resistance toggled to {}", enabled ? "ON" : "OFF");
}

void PhysicsControls::onAirResistanceChanged(lv_event_t* e)
{
    lv_obj_t* target = static_cast<lv_obj_t*>(lv_event_get_target(e));
    PhysicsControls* self = static_cast<PhysicsControls*>(lv_obj_get_user_data(target));
    if (!self) return;

    int value = lv_slider_get_value(target);
    double scaledValue = value * 0.01;
    self->sendPhysicsCommand("air_resistance_set", scaledValue);
}

// Column 2: Pressure event handlers.
void PhysicsControls::onHydrostaticPressureToggled(lv_event_t* e)
{
    lv_obj_t* target = static_cast<lv_obj_t*>(lv_event_get_target(e));
    PhysicsControls* self = static_cast<PhysicsControls*>(lv_obj_get_user_data(target));
    if (!self) return;

    bool enabled = lv_obj_has_state(target, LV_STATE_CHECKED);
    spdlog::info("PhysicsControls: Hydrostatic Pressure toggled to {}", enabled ? "ON" : "OFF");
}

void PhysicsControls::onHydrostaticPressureChanged(lv_event_t* e)
{
    lv_obj_t* target = static_cast<lv_obj_t*>(lv_event_get_target(e));
    PhysicsControls* self = static_cast<PhysicsControls*>(lv_obj_get_user_data(target));
    if (!self) return;

    int value = lv_slider_get_value(target);
    double scaledValue = value * 0.01;
    self->sendPhysicsCommand("hydrostatic_pressure_set", scaledValue);
}

void PhysicsControls::onDynamicPressureToggled(lv_event_t* e)
{
    lv_obj_t* target = static_cast<lv_obj_t*>(lv_event_get_target(e));
    PhysicsControls* self = static_cast<PhysicsControls*>(lv_obj_get_user_data(target));
    if (!self) return;

    bool enabled = lv_obj_has_state(target, LV_STATE_CHECKED);
    spdlog::info("PhysicsControls: Dynamic Pressure toggled to {}", enabled ? "ON" : "OFF");
}

void PhysicsControls::onDynamicPressureChanged(lv_event_t* e)
{
    lv_obj_t* target = static_cast<lv_obj_t*>(lv_event_get_target(e));
    PhysicsControls* self = static_cast<PhysicsControls*>(lv_obj_get_user_data(target));
    if (!self) return;

    int value = lv_slider_get_value(target);
    double scaledValue = value * 0.01;
    self->sendPhysicsCommand("dynamic_pressure_set", scaledValue);
}

void PhysicsControls::onPressureDiffusionToggled(lv_event_t* e)
{
    lv_obj_t* target = static_cast<lv_obj_t*>(lv_event_get_target(e));
    PhysicsControls* self = static_cast<PhysicsControls*>(lv_obj_get_user_data(target));
    if (!self) return;

    bool enabled = lv_obj_has_state(target, LV_STATE_CHECKED);
    spdlog::info("PhysicsControls: Pressure Diffusion toggled to {}", enabled ? "ON" : "OFF");
}

void PhysicsControls::onPressureDiffusionChanged(lv_event_t* e)
{
    lv_obj_t* target = static_cast<lv_obj_t*>(lv_event_get_target(e));
    PhysicsControls* self = static_cast<PhysicsControls*>(lv_obj_get_user_data(target));
    if (!self) return;

    int value = lv_slider_get_value(target);
    double scaledValue = value * 0.01;
    self->sendPhysicsCommand("pressure_diffusion_set", scaledValue);
}

// Column 3: Forces event handlers.
void PhysicsControls::onCohesionForceToggled(lv_event_t* e)
{
    lv_obj_t* target = static_cast<lv_obj_t*>(lv_event_get_target(e));
    PhysicsControls* self = static_cast<PhysicsControls*>(lv_obj_get_user_data(target));
    if (!self) return;

    bool enabled = lv_obj_has_state(target, LV_STATE_CHECKED);
    spdlog::info("PhysicsControls: Cohesion Force toggled to {}", enabled ? "ON" : "OFF");
}

void PhysicsControls::onCohesionForceChanged(lv_event_t* e)
{
    lv_obj_t* target = static_cast<lv_obj_t*>(lv_event_get_target(e));
    PhysicsControls* self = static_cast<PhysicsControls*>(lv_obj_get_user_data(target));
    if (!self) return;

    int value = lv_slider_get_value(target);
    double scaledValue = value * 0.01;
    self->sendPhysicsCommand("cohesion_force_set", scaledValue);
}

void PhysicsControls::onAdhesionToggled(lv_event_t* e)
{
    lv_obj_t* target = static_cast<lv_obj_t*>(lv_event_get_target(e));
    PhysicsControls* self = static_cast<PhysicsControls*>(lv_obj_get_user_data(target));
    if (!self) return;

    bool enabled = lv_obj_has_state(target, LV_STATE_CHECKED);
    spdlog::info("PhysicsControls: Adhesion toggled to {}", enabled ? "ON" : "OFF");
}

void PhysicsControls::onAdhesionChanged(lv_event_t* e)
{
    lv_obj_t* target = static_cast<lv_obj_t*>(lv_event_get_target(e));
    PhysicsControls* self = static_cast<PhysicsControls*>(lv_obj_get_user_data(target));
    if (!self) return;

    int value = lv_slider_get_value(target);
    double scaledValue = value * 0.01;
    self->sendPhysicsCommand("adhesion_set", scaledValue);
}

void PhysicsControls::onViscosityToggled(lv_event_t* e)
{
    lv_obj_t* target = static_cast<lv_obj_t*>(lv_event_get_target(e));
    PhysicsControls* self = static_cast<PhysicsControls*>(lv_obj_get_user_data(target));
    if (!self) return;

    bool enabled = lv_obj_has_state(target, LV_STATE_CHECKED);
    spdlog::info("PhysicsControls: Viscosity toggled to {}", enabled ? "ON" : "OFF");
}

void PhysicsControls::onViscosityChanged(lv_event_t* e)
{
    lv_obj_t* target = static_cast<lv_obj_t*>(lv_event_get_target(e));
    PhysicsControls* self = static_cast<PhysicsControls*>(lv_obj_get_user_data(target));
    if (!self) return;

    int value = lv_slider_get_value(target);
    double scaledValue = value * 0.01;
    self->sendPhysicsCommand("viscosity_set", scaledValue);
}

void PhysicsControls::onFrictionToggled(lv_event_t* e)
{
    lv_obj_t* target = static_cast<lv_obj_t*>(lv_event_get_target(e));
    PhysicsControls* self = static_cast<PhysicsControls*>(lv_obj_get_user_data(target));
    if (!self) return;

    bool enabled = lv_obj_has_state(target, LV_STATE_CHECKED);
    spdlog::info("PhysicsControls: Friction toggled to {}", enabled ? "ON" : "OFF");
}

void PhysicsControls::onFrictionChanged(lv_event_t* e)
{
    lv_obj_t* target = static_cast<lv_obj_t*>(lv_event_get_target(e));
    PhysicsControls* self = static_cast<PhysicsControls*>(lv_obj_get_user_data(target));
    if (!self) return;

    int value = lv_slider_get_value(target);
    double scaledValue = value * 0.01;
    self->sendPhysicsCommand("friction_set", scaledValue);
}

} // namespace Ui
} // namespace DirtSim
