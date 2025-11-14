#include "PhysicsControls.h"
#include "server/api/PhysicsSettingsSet.h"
#include "ui/state-machine/network/WebSocketClient.h"
#include "ui/ui_builders/LVGLBuilder.h"
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

namespace DirtSim {
namespace Ui {

PhysicsControls::PhysicsControls(lv_obj_t* container, WebSocketClient* wsClient)
    : container_(container), wsClient_(wsClient)
{
    // Create 3-column layout.
    lv_obj_set_flex_flow(container_, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(
        container_, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

    // Column 1: General Physics.
    column1_ = lv_obj_create(container_);
    lv_obj_set_size(column1_, LV_PCT(30), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(column1_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(
        column1_, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(column1_, 4, 0);
    lv_obj_set_style_pad_all(column1_, 8, 0);

    lv_obj_t* label1 = lv_label_create(column1_);
    lv_label_set_text(label1, "General Physics");
    lv_obj_set_style_text_font(label1, &lv_font_montserrat_14, 0);

    timescaleControl_ = LVGLBuilder::toggleSlider(column1_)
                            .label("Timescale")
                            .range(0, 500)
                            .value(100)
                            .defaultValue(100)
                            .valueScale(0.01)
                            .valueFormat("%.2fx")
                            .initiallyEnabled(true)
                            .sliderWidth(180)
                            .onToggle(onTimescaleToggled, this)
                            .onSliderChange(onTimescaleChanged, this)
                            .buildOrLog();

    gravityControl_ = LVGLBuilder::toggleSlider(column1_)
                          .label("Gravity")
                          .range(-5000, 5000)
                          .value(981)
                          .defaultValue(981)
                          .valueScale(0.01)
                          .valueFormat("%.2f")
                          .initiallyEnabled(true)
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
                             .initiallyEnabled(true)
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
                                .initiallyEnabled(true)
                                .sliderWidth(180)
                                .onToggle(onAirResistanceToggled, this)
                                .onSliderChange(onAirResistanceChanged, this)
                                .buildOrLog();

    swapEnabledControl_ = LVGLBuilder::labeledSwitch(column1_)
                              .label("Enable Swap")
                              .initialState(false)
                              .callback(onSwapEnabledToggled, this)
                              .buildOrLog();

    // Column 2: Pressure.
    column2_ = lv_obj_create(container_);
    lv_obj_set_size(column2_, LV_PCT(30), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(column2_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(
        column2_, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
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
                                    .range(0, 500)
                                    .value(100)
                                    .defaultValue(100)
                                    .valueScale(0.01)
                                    .valueFormat("%.2f")
                                    .initiallyEnabled(true)
                                    .sliderWidth(180)
                                    .onToggle(onPressureDiffusionToggled, this)
                                    .onSliderChange(onPressureDiffusionChanged, this)
                                    .buildOrLog();

    pressureScaleControl_ = LVGLBuilder::toggleSlider(column2_)
                                .label("Scale")
                                .range(0, 500)
                                .value(100)
                                .defaultValue(100)
                                .valueScale(0.01)
                                .valueFormat("%.2f")
                                .initiallyEnabled(true)
                                .sliderWidth(180)
                                .onToggle(onPressureScaleToggled, this)
                                .onSliderChange(onPressureScaleChanged, this)
                                .buildOrLog();

    // Column 3: Forces.
    column3_ = lv_obj_create(container_);
    lv_obj_set_size(column3_, LV_PCT(30), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(column3_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(
        column3_, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
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

    // Fetch initial settings from server.
    fetchSettings();

    spdlog::info("PhysicsControls: Initialized");
}

PhysicsControls::~PhysicsControls()
{
    spdlog::info("PhysicsControls: Destroyed");
}

void PhysicsControls::updateFromSettings(const PhysicsSettings& settings)
{
    spdlog::info("PhysicsControls: Updating UI from server settings");

    // Update local settings copy.
    settings_ = settings;

    // Helper to update a toggle slider control.
    auto updateToggleSlider = [](lv_obj_t* control, double value, bool enabled) {
        if (!control) return;

        // Control is a container with toggle (child 0) and slider (child 2).
        lv_obj_t* toggle = lv_obj_get_child(control, 0);
        lv_obj_t* slider = lv_obj_get_child(control, 2);

        if (toggle) {
            if (enabled) {
                lv_obj_add_state(toggle, LV_STATE_CHECKED);
            }
            else {
                lv_obj_remove_state(toggle, LV_STATE_CHECKED);
            }
        }

        if (slider) {
            // Convert double value back to integer slider value (reverse the 0.01 scale).
            int sliderValue = static_cast<int>(value * 100.0);
            lv_slider_set_value(slider, sliderValue, LV_ANIM_OFF);
        }
    };

    // Update Column 1: General Physics.
    updateToggleSlider(timescaleControl_, settings.timescale, settings.timescale > 0.0);
    updateToggleSlider(gravityControl_, settings.gravity, true); // No enabled flag for gravity.
    updateToggleSlider(elasticityControl_, settings.elasticity, true);        // No enabled flag.
    updateToggleSlider(airResistanceControl_, settings.air_resistance, true); // No enabled flag.

    // Update Column 2: Pressure.
    updateToggleSlider(
        hydrostaticPressureControl_,
        settings.pressure_hydrostatic_strength,
        settings.pressure_hydrostatic_enabled);
    updateToggleSlider(
        dynamicPressureControl_,
        settings.pressure_dynamic_strength,
        settings.pressure_dynamic_enabled);
    updateToggleSlider(
        pressureDiffusionControl_, settings.pressure_diffusion_strength, true); // No enabled flag.
    updateToggleSlider(pressureScaleControl_, settings.pressure_scale, true);   // No enabled flag.

    // Update Column 3: Forces.
    updateToggleSlider(
        cohesionForceControl_, settings.cohesion_strength, settings.cohesion_enabled);
    updateToggleSlider(adhesionControl_, settings.adhesion_strength, settings.adhesion_enabled);
    updateToggleSlider(viscosityControl_, settings.viscosity_strength, settings.viscosity_enabled);
    updateToggleSlider(frictionControl_, settings.friction_strength, settings.friction_enabled);

    // Update swap toggle.
    if (swapEnabledControl_) {
        if (settings.swap_enabled) {
            lv_obj_add_state(swapEnabledControl_, LV_STATE_CHECKED);
        }
        else {
            lv_obj_remove_state(swapEnabledControl_, LV_STATE_CHECKED);
        }
    }

    spdlog::info("PhysicsControls: UI updated from server settings");
}

// Column 1: General Physics event handlers.
void PhysicsControls::onTimescaleToggled(lv_event_t* e)
{
    lv_obj_t* target = static_cast<lv_obj_t*>(lv_event_get_target(e));
    PhysicsControls* self = static_cast<PhysicsControls*>(lv_obj_get_user_data(target));
    if (!self) return;

    bool enabled = lv_obj_has_state(target, LV_STATE_CHECKED);
    spdlog::info("PhysicsControls: Timescale toggled to {}", enabled ? "ON" : "OFF");

    if (!enabled) {
        // When disabled, set timescale to 0 to pause simulation time.
        self->settings_.timescale = 0.0;
        self->syncSettings();
    }
    else {
        // When re-enabled, LVGLBuilder has restored the slider value.
        // Read it from the slider and sync to server.
        // Note: We need to access the slider widget, which is a sibling of the switch.
        lv_obj_t* container = lv_obj_get_parent(target);
        if (container) {
            // Find the slider child (it's the 3rd child: label, switch, slider, value).
            lv_obj_t* slider = lv_obj_get_child(container, 2);
            if (slider) {
                int value = lv_slider_get_value(slider);
                double scaledValue = value * 0.01;
                self->settings_.timescale = scaledValue;
                self->syncSettings();
                spdlog::debug("PhysicsControls: Restored timescale to {:.2f}", scaledValue);
            }
        }
    }
}

void PhysicsControls::onTimescaleChanged(lv_event_t* e)
{
    lv_obj_t* target = static_cast<lv_obj_t*>(lv_event_get_target(e));
    PhysicsControls* self = static_cast<PhysicsControls*>(lv_obj_get_user_data(target));
    if (!self) return;

    int value = lv_slider_get_value(target);
    double scaledValue = value * 0.01;

    spdlog::info("PhysicsControls: Timescale changed to {:.2f}", scaledValue);

    self->settings_.timescale = scaledValue;
    self->syncSettings();
}

void PhysicsControls::onGravityToggled(lv_event_t* e)
{
    lv_obj_t* target = static_cast<lv_obj_t*>(lv_event_get_target(e));
    PhysicsControls* self = static_cast<PhysicsControls*>(lv_obj_get_user_data(target));
    if (!self) return;

    bool enabled = lv_obj_has_state(target, LV_STATE_CHECKED);
    spdlog::info("PhysicsControls: Gravity toggled to {}", enabled ? "ON" : "OFF");

    if (!enabled) {
        // When disabled, set gravity to 0.
        self->settings_.gravity = 0.0;
        self->syncSettings();
    }
    else {
        // When re-enabled, LVGLBuilder has restored the slider value.
        // Read it from the slider and sync to server.
        lv_obj_t* container = lv_obj_get_parent(target);
        if (container) {
            // Find the slider child (it's the 3rd child: label, switch, slider, value).
            lv_obj_t* slider = lv_obj_get_child(container, 2);
            if (slider) {
                int value = lv_slider_get_value(slider);
                double scaledValue = value * 0.01;
                self->settings_.gravity = scaledValue;
                self->syncSettings();
                spdlog::debug("PhysicsControls: Restored gravity to {:.2f}", scaledValue);
            }
        }
    }
}

void PhysicsControls::onGravityChanged(lv_event_t* e)
{
    lv_obj_t* target = static_cast<lv_obj_t*>(lv_event_get_target(e));
    PhysicsControls* self = static_cast<PhysicsControls*>(lv_obj_get_user_data(target));
    if (!self) return;

    int value = lv_slider_get_value(target);
    double scaledValue = value * 0.01;
    spdlog::info("PhysicsControls: Gravity changed to {:.2f}", scaledValue);
    self->settings_.gravity = scaledValue;
    self->syncSettings();
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
    spdlog::info("PhysicsControls: Elasticity changed to {:.2f}", scaledValue);
    self->settings_.elasticity = scaledValue;
    self->syncSettings();
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
    spdlog::info("PhysicsControls: Air Resistance changed to {:.2f}", scaledValue);
    self->settings_.air_resistance = scaledValue;
    self->syncSettings();
}

void PhysicsControls::onSwapEnabledToggled(lv_event_t* e)
{
    lv_obj_t* target = static_cast<lv_obj_t*>(lv_event_get_target(e));
    PhysicsControls* self = static_cast<PhysicsControls*>(lv_obj_get_user_data(target));
    if (!self) return;

    bool enabled = lv_obj_has_state(target, LV_STATE_CHECKED);
    spdlog::info("PhysicsControls: Enable Swap toggled to {}", enabled ? "ON" : "OFF");
    self->settings_.swap_enabled = enabled;
    self->syncSettings();
}

// Column 2: Pressure event handlers.
void PhysicsControls::onHydrostaticPressureToggled(lv_event_t* e)
{
    lv_obj_t* target = static_cast<lv_obj_t*>(lv_event_get_target(e));
    PhysicsControls* self = static_cast<PhysicsControls*>(lv_obj_get_user_data(target));
    if (!self) return;

    bool enabled = lv_obj_has_state(target, LV_STATE_CHECKED);
    spdlog::info("PhysicsControls: Hydrostatic Pressure toggled to {}", enabled ? "ON" : "OFF");

    if (!enabled) {
        self->settings_.pressure_hydrostatic_strength = 0.0;
        self->syncSettings();
    }
    else {
        lv_obj_t* container = lv_obj_get_parent(target);
        if (container) {
            lv_obj_t* slider = lv_obj_get_child(container, 2);
            if (slider) {
                int value = lv_slider_get_value(slider);
                double scaledValue = value * 0.01;
                self->settings_.pressure_hydrostatic_strength = scaledValue;
                self->syncSettings();
                spdlog::debug(
                    "PhysicsControls: Restored hydrostatic pressure to {:.2f}", scaledValue);
            }
        }
    }
}

void PhysicsControls::onHydrostaticPressureChanged(lv_event_t* e)
{
    lv_obj_t* target = static_cast<lv_obj_t*>(lv_event_get_target(e));
    PhysicsControls* self = static_cast<PhysicsControls*>(lv_obj_get_user_data(target));
    if (!self) return;

    int value = lv_slider_get_value(target);
    double scaledValue = value * 0.01;
    spdlog::info("PhysicsControls: Hydrostatic Pressure changed to {:.2f}", scaledValue);
    self->settings_.pressure_hydrostatic_strength = scaledValue;
    self->syncSettings();
}

void PhysicsControls::onDynamicPressureToggled(lv_event_t* e)
{
    lv_obj_t* target = static_cast<lv_obj_t*>(lv_event_get_target(e));
    PhysicsControls* self = static_cast<PhysicsControls*>(lv_obj_get_user_data(target));
    if (!self) return;

    bool enabled = lv_obj_has_state(target, LV_STATE_CHECKED);
    spdlog::info("PhysicsControls: Dynamic Pressure toggled to {}", enabled ? "ON" : "OFF");

    if (!enabled) {
        self->settings_.pressure_dynamic_strength = 0.0;
        self->syncSettings();
    }
    else {
        lv_obj_t* container = lv_obj_get_parent(target);
        if (container) {
            lv_obj_t* slider = lv_obj_get_child(container, 2);
            if (slider) {
                int value = lv_slider_get_value(slider);
                double scaledValue = value * 0.01;
                self->settings_.pressure_dynamic_strength = scaledValue;
                self->syncSettings();
                spdlog::debug("PhysicsControls: Restored dynamic pressure to {:.2f}", scaledValue);
            }
        }
    }
}

void PhysicsControls::onDynamicPressureChanged(lv_event_t* e)
{
    lv_obj_t* target = static_cast<lv_obj_t*>(lv_event_get_target(e));
    PhysicsControls* self = static_cast<PhysicsControls*>(lv_obj_get_user_data(target));
    if (!self) return;

    int value = lv_slider_get_value(target);
    double scaledValue = value * 0.01;
    spdlog::info("PhysicsControls: Dynamic Pressure changed to {:.2f}", scaledValue);
    self->settings_.pressure_dynamic_strength = scaledValue;
    self->syncSettings();
}

void PhysicsControls::onPressureDiffusionToggled(lv_event_t* e)
{
    lv_obj_t* target = static_cast<lv_obj_t*>(lv_event_get_target(e));
    PhysicsControls* self = static_cast<PhysicsControls*>(lv_obj_get_user_data(target));
    if (!self) return;

    bool enabled = lv_obj_has_state(target, LV_STATE_CHECKED);
    spdlog::info("PhysicsControls: Pressure Diffusion toggled to {}", enabled ? "ON" : "OFF");

    if (!enabled) {
        self->settings_.pressure_diffusion_strength = 0.0;
        self->syncSettings();
    }
    else {
        lv_obj_t* container = lv_obj_get_parent(target);
        if (container) {
            lv_obj_t* slider = lv_obj_get_child(container, 2);
            if (slider) {
                int value = lv_slider_get_value(slider);
                double scaledValue = value * 0.01;
                self->settings_.pressure_diffusion_strength = scaledValue;
                self->syncSettings();
                spdlog::debug(
                    "PhysicsControls: Restored pressure diffusion to {:.2f}", scaledValue);
            }
        }
    }
}

void PhysicsControls::onPressureDiffusionChanged(lv_event_t* e)
{
    lv_obj_t* target = static_cast<lv_obj_t*>(lv_event_get_target(e));
    PhysicsControls* self = static_cast<PhysicsControls*>(lv_obj_get_user_data(target));
    if (!self) return;

    int value = lv_slider_get_value(target);
    double scaledValue = value * 0.01;
    spdlog::info("PhysicsControls: Pressure Diffusion changed to {:.2f}", scaledValue);
    self->settings_.pressure_diffusion_strength = scaledValue;
    self->syncSettings();
}

void PhysicsControls::onPressureScaleToggled(lv_event_t* e)
{
    lv_obj_t* target = static_cast<lv_obj_t*>(lv_event_get_target(e));
    PhysicsControls* self = static_cast<PhysicsControls*>(lv_obj_get_user_data(target));
    if (!self) return;

    bool enabled = lv_obj_has_state(target, LV_STATE_CHECKED);
    spdlog::info("PhysicsControls: Pressure Scale toggled to {}", enabled ? "ON" : "OFF");

    if (!enabled) {
        self->settings_.pressure_scale = 0.0;
        self->syncSettings();
    }
    else {
        // Restore value from slider when re-enabled.
        lv_obj_t* container = lv_obj_get_parent(target);
        if (container) {
            lv_obj_t* slider = lv_obj_get_child(container, 2);
            if (slider) {
                int value = lv_slider_get_value(slider);
                double scaledValue = value * 0.01;
                self->settings_.pressure_scale = scaledValue;
                self->syncSettings();
                spdlog::debug("PhysicsControls: Restored pressure scale to {:.2f}", scaledValue);
            }
        }
    }
}

void PhysicsControls::onPressureScaleChanged(lv_event_t* e)
{
    lv_obj_t* target = static_cast<lv_obj_t*>(lv_event_get_target(e));
    PhysicsControls* self = static_cast<PhysicsControls*>(lv_obj_get_user_data(target));
    if (!self) return;

    int value = lv_slider_get_value(target);
    double scaledValue = value * 0.01;
    spdlog::info("PhysicsControls: Pressure Scale changed to {:.2f}", scaledValue);
    self->settings_.pressure_scale = scaledValue;
    self->syncSettings();
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
    spdlog::info("PhysicsControls: Cohesion Force changed to {:.1f}", scaledValue);
    self->settings_.cohesion_strength = scaledValue;
    self->syncSettings();
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
    spdlog::info("PhysicsControls: Adhesion changed to {:.1f}", scaledValue);
    self->settings_.adhesion_strength = scaledValue;
    self->syncSettings();
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
    spdlog::info("PhysicsControls: Viscosity changed to {:.2f}", scaledValue);
    self->settings_.viscosity_strength = scaledValue;
    self->syncSettings();
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

    spdlog::info("PhysicsControls: Friction changed to {:.2f}", scaledValue);

    self->settings_.friction_strength = scaledValue;
    self->syncSettings();
}

void PhysicsControls::fetchSettings()
{
    if (!wsClient_ || !wsClient_->isConnected()) {
        spdlog::warn("PhysicsControls: Cannot fetch settings - not connected");
        return;
    }

    spdlog::info("PhysicsControls: Fetching physics settings from server");

    nlohmann::json cmd;
    cmd["command"] = "physics_settings_get";
    wsClient_->send(cmd.dump());

    // Response will be handled by UI state machine and used to update controls.
    // For now, we just use default values.
}

void PhysicsControls::syncSettings()
{
    if (!wsClient_ || !wsClient_->isConnected()) {
        spdlog::warn("PhysicsControls: Cannot sync settings - not connected");
        return;
    }

    spdlog::debug("PhysicsControls: Syncing physics settings to server");

    Api::PhysicsSettingsSet::Command cmd;
    cmd.settings = settings_;

    nlohmann::json j = cmd.toJson();
    j["command"] = "physics_settings_set";

    wsClient_->send(j.dump());
}

} // namespace Ui
} // namespace DirtSim
