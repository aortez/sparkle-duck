#include "PhysicsControls.h"
#include "server/api/PhysicsSettingsGet.h"
#include "server/api/PhysicsSettingsSet.h"
#include "ui/state-machine/network/WebSocketClient.h"
#include "ui/ui_builders/LVGLBuilder.h"
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <string>

namespace DirtSim {
namespace Ui {

// Static function to create all control configurations using brace initialization.
std::vector<PhysicsControls::ColumnConfig> PhysicsControls::createColumnConfigs()
{
    return {
        // Column 1: General Physics.
        { .title = "General Physics",
          .controls = { { .label = "Timescale",
                          .type = ControlType::TOGGLE_SLIDER,
                          .rangeMin = -500,
                          .rangeMax = 1000,
                          .defaultValue = 100,
                          .valueScale = 0.01,
                          .valueFormat = "%.2fx",
                          .initiallyEnabled = true,
                          .valueSetter = [](PhysicsSettings& s, double v) { s.timescale = v; },
                          .valueGetter = [](const PhysicsSettings& s) { return s.timescale; },
                          .enableSetter =
                              [](PhysicsSettings& s, bool e) {
                                  // Special case: timescale doesn't have separate enable flag.
                                  // When disabled, we set it to 0.
                                  if (!e) s.timescale = 0.0;
                              },
                          .enableGetter =
                              [](const PhysicsSettings& s) { return s.timescale > 0.0; } },
                        { .label = "Gravity",
                          .type = ControlType::TOGGLE_SLIDER,
                          .rangeMin = -5000,
                          .rangeMax = 50000,
                          .defaultValue = 981,
                          .valueScale = 0.01,
                          .valueFormat = "%.2f",
                          .initiallyEnabled = true,
                          .valueSetter = [](PhysicsSettings& s, double v) { s.gravity = v; },
                          .valueGetter = [](const PhysicsSettings& s) { return s.gravity; },
                          .enableSetter =
                              [](PhysicsSettings& s, bool e) {
                                  // Special case: gravity doesn't have separate enable flag.
                                  // When disabled, we set it to 0.
                                  if (!e) s.gravity = 0.0;
                              },
                          .enableGetter =
                              [](const PhysicsSettings& s) {
                                  // Consider gravity enabled if it's non-zero.
                                  // This handles both positive and negative gravity.
                                  return s.gravity != 0.0;
                              } },
                        { .label = "Elasticity",
                          .type = ControlType::TOGGLE_SLIDER,
                          .rangeMin = 0,
                          .rangeMax = 100,
                          .defaultValue = 80,
                          .valueScale = 0.01,
                          .valueFormat = "%.2f",
                          .initiallyEnabled = true,
                          .valueSetter = [](PhysicsSettings& s, double v) { s.elasticity = v; },
                          .valueGetter = [](const PhysicsSettings& s) { return s.elasticity; },
                          .enableSetter =
                              []([[maybe_unused]] PhysicsSettings& s, [[maybe_unused]] bool e) {
                                  // Elasticity doesn't disable, just log the toggle.
                              },
                          .enableGetter =
                              []([[maybe_unused]] const PhysicsSettings& s) { return true; } },
                        { .label = "Air Resistance",
                          .type = ControlType::TOGGLE_SLIDER,
                          .rangeMin = 0,
                          .rangeMax = 100,
                          .defaultValue = 10,
                          .valueScale = 0.01,
                          .valueFormat = "%.2f",
                          .initiallyEnabled = true,
                          .valueSetter = [](PhysicsSettings& s, double v) { s.air_resistance = v; },
                          .valueGetter = [](const PhysicsSettings& s) { return s.air_resistance; },
                          .enableSetter =
                              []([[maybe_unused]] PhysicsSettings& s, [[maybe_unused]] bool e) {
                                  // Air resistance doesn't disable, just log the toggle.
                              },
                          .enableGetter =
                              []([[maybe_unused]] const PhysicsSettings& s) { return true; } },
                        { .label = "Enable Swap",
                          .type = ControlType::SWITCH_ONLY,
                          .enableSetter = [](PhysicsSettings& s, bool e) { s.swap_enabled = e; },
                          .enableGetter =
                              [](const PhysicsSettings& s) { return s.swap_enabled; } } } },
        // Column 2: Pressure.
        { .title = "Pressure",
          .controls = { { .label = "Hydrostatic",
                          .type = ControlType::TOGGLE_SLIDER,
                          .rangeMin = 0,
                          .rangeMax = 300,
                          .defaultValue = 100,
                          .valueScale = 0.01,
                          .valueFormat = "%.2f",
                          .initiallyEnabled = true,
                          .valueSetter = [](PhysicsSettings& s,
                                            double v) { s.pressure_hydrostatic_strength = v; },
                          .valueGetter =
                              [](const PhysicsSettings& s) {
                                  return s.pressure_hydrostatic_strength;
                              },
                          .enableSetter =
                              [](PhysicsSettings& s, bool e) {
                                  s.pressure_hydrostatic_enabled = e;
                                  if (!e) s.pressure_hydrostatic_strength = 0.0;
                              },
                          .enableGetter =
                              [](const PhysicsSettings& s) {
                                  return s.pressure_hydrostatic_enabled;
                              } },
                        { .label = "Dynamic",
                          .type = ControlType::TOGGLE_SLIDER,
                          .rangeMin = 0,
                          .rangeMax = 300,
                          .defaultValue = 100,
                          .valueScale = 0.01,
                          .valueFormat = "%.2f",
                          .initiallyEnabled = true,
                          .valueSetter = [](PhysicsSettings& s,
                                            double v) { s.pressure_dynamic_strength = v; },
                          .valueGetter =
                              [](const PhysicsSettings& s) { return s.pressure_dynamic_strength; },
                          .enableSetter =
                              [](PhysicsSettings& s, bool e) {
                                  s.pressure_dynamic_enabled = e;
                                  if (!e) s.pressure_dynamic_strength = 0.0;
                              },
                          .enableGetter =
                              [](const PhysicsSettings& s) { return s.pressure_dynamic_enabled; } },
                        { .label = "Diffusion",
                          .type = ControlType::TOGGLE_SLIDER,
                          .rangeMin = 0,
                          .rangeMax = 50000,
                          .defaultValue = 500,
                          .valueScale = 0.01,
                          .valueFormat = "%.2f",
                          .initiallyEnabled = true,
                          .valueSetter = [](PhysicsSettings& s,
                                            double v) { s.pressure_diffusion_strength = v; },
                          .valueGetter =
                              [](const PhysicsSettings& s) {
                                  return s.pressure_diffusion_strength;
                              },
                          .enableSetter =
                              [](PhysicsSettings& s, bool e) {
                                  if (!e) s.pressure_diffusion_strength = 0.0;
                              },
                          .enableGetter =
                              [](const PhysicsSettings& s) {
                                  return s.pressure_diffusion_strength > 0.0;
                              } },
                        { .label = "Diffusion Iters",
                          .type = ControlType::TOGGLE_SLIDER,
                          .rangeMin = 1,
                          .rangeMax = 5,
                          .defaultValue = 1,
                          .valueScale = 1.0,
                          .valueFormat = "%.0f",
                          .initiallyEnabled = true,
                          .valueSetter =
                              [](PhysicsSettings& s, double v) {
                                  s.pressure_diffusion_iterations = static_cast<int>(v);
                              },
                          .valueGetter =
                              [](const PhysicsSettings& s) {
                                  return static_cast<double>(s.pressure_diffusion_iterations);
                              },
                          .enableSetter = []([[maybe_unused]] PhysicsSettings& s,
                                             [[maybe_unused]] bool e) {},
                          .enableGetter =
                              []([[maybe_unused]] const PhysicsSettings& s) { return true; } },
                        { .label = "Scale",
                          .type = ControlType::TOGGLE_SLIDER,
                          .rangeMin = 0,
                          .rangeMax = 500,
                          .defaultValue = 100,
                          .valueScale = 0.01,
                          .valueFormat = "%.2f",
                          .initiallyEnabled = true,
                          .valueSetter = [](PhysicsSettings& s, double v) { s.pressure_scale = v; },
                          .valueGetter = [](const PhysicsSettings& s) { return s.pressure_scale; },
                          .enableSetter =
                              [](PhysicsSettings& s, bool e) {
                                  if (!e) s.pressure_scale = 0.0;
                              },
                          .enableGetter =
                              [](const PhysicsSettings& s) { return s.pressure_scale > 0.0; } } } },
        // Column 3: Forces.
        { .title = "Forces",
          .controls = { { .label = "Cohesion",
                          .type = ControlType::TOGGLE_SLIDER,
                          .rangeMin = 0,
                          .rangeMax = 2000,
                          .defaultValue = 0,
                          .valueScale = 0.01,
                          .valueFormat = "%.0f",
                          .initiallyEnabled = true,
                          .valueSetter = [](PhysicsSettings& s,
                                            double v) { s.cohesion_strength = v; },
                          .valueGetter =
                              [](const PhysicsSettings& s) { return s.cohesion_strength; },
                          .enableSetter =
                              [](PhysicsSettings& s, bool e) {
                                  s.cohesion_enabled = e;
                                  if (!e) s.cohesion_strength = 0.0;
                              },
                          .enableGetter =
                              [](const PhysicsSettings& s) { return s.cohesion_enabled; } },
                        { .label = "Adhesion",
                          .type = ControlType::TOGGLE_SLIDER,
                          .rangeMin = 0,
                          .rangeMax = 1000,
                          .defaultValue = 500,
                          .valueScale = 0.01,
                          .valueFormat = "%.1f",
                          .initiallyEnabled = true,
                          .valueSetter = [](PhysicsSettings& s,
                                            double v) { s.adhesion_strength = v; },
                          .valueGetter =
                              [](const PhysicsSettings& s) { return s.adhesion_strength; },
                          .enableSetter =
                              [](PhysicsSettings& s, bool e) {
                                  s.adhesion_enabled = e;
                                  if (!e) s.adhesion_strength = 0.0;
                              },
                          .enableGetter =
                              [](const PhysicsSettings& s) { return s.adhesion_enabled; } },
                        { .label = "Viscosity",
                          .type = ControlType::TOGGLE_SLIDER,
                          .rangeMin = 0,
                          .rangeMax = 1000,
                          .defaultValue = 100,
                          .valueScale = 0.01,
                          .valueFormat = "%.2f",
                          .initiallyEnabled = true,
                          .valueSetter = [](PhysicsSettings& s,
                                            double v) { s.viscosity_strength = v; },
                          .valueGetter =
                              [](const PhysicsSettings& s) { return s.viscosity_strength; },
                          .enableSetter =
                              [](PhysicsSettings& s, bool e) {
                                  s.viscosity_enabled = e;
                                  if (!e) s.viscosity_strength = 0.0;
                              },
                          .enableGetter =
                              [](const PhysicsSettings& s) { return s.viscosity_enabled; } },
                        { .label = "Friction",
                          .type = ControlType::TOGGLE_SLIDER,
                          .rangeMin = 0,
                          .rangeMax = 200,
                          .defaultValue = 100,
                          .valueScale = 0.01,
                          .valueFormat = "%.2f",
                          .initiallyEnabled = true,
                          .valueSetter = [](PhysicsSettings& s,
                                            double v) { s.friction_strength = v; },
                          .valueGetter =
                              [](const PhysicsSettings& s) { return s.friction_strength; },
                          .enableSetter =
                              [](PhysicsSettings& s, bool e) {
                                  s.friction_enabled = e;
                                  if (!e) s.friction_strength = 0.0;
                              },
                          .enableGetter =
                              [](const PhysicsSettings& s) { return s.friction_enabled; } },
                        { .label = "Cohesion Resist",
                          .type = ControlType::TOGGLE_SLIDER,
                          .rangeMin = 0,
                          .rangeMax = 100,
                          .defaultValue = 10,
                          .valueScale = 1.0,
                          .valueFormat = "%.0f",
                          .initiallyEnabled = true,
                          .valueSetter =
                              [](PhysicsSettings& s, double v) {
                                  s.cohesion_resistance_factor = v;
                              },
                          .valueGetter =
                              [](const PhysicsSettings& s) { return s.cohesion_resistance_factor; },
                          .enableSetter =
                              [](PhysicsSettings& s, bool e) {
                                  if (!e) s.cohesion_resistance_factor = 0.0;
                              },
                          .enableGetter =
                              [](const PhysicsSettings& s) {
                                  return s.cohesion_resistance_factor > 0.0;
                              } } } },
        // Column 4: Swap Tuning.
        { .title = "Swap Tuning",
          .controls = { { .label = "Buoyancy Energy",
                          .type = ControlType::TOGGLE_SLIDER,
                          .rangeMin = 0,
                          .rangeMax = 2000,
                          .defaultValue = 500,
                          .valueScale = 0.01,
                          .valueFormat = "%.1f",
                          .initiallyEnabled = true,
                          .valueSetter = [](PhysicsSettings& s,
                                            double v) { s.buoyancy_energy_scale = v; },
                          .valueGetter =
                              [](const PhysicsSettings& s) { return s.buoyancy_energy_scale; },
                          .enableSetter = []([[maybe_unused]] PhysicsSettings& s,
                                             [[maybe_unused]] bool e) {},
                          .enableGetter =
                              []([[maybe_unused]] const PhysicsSettings& s) { return true; } },
                        { .label = "Cohesion Bonds",
                          .type = ControlType::TOGGLE_SLIDER,
                          .rangeMin = 0,
                          .rangeMax = 5000,
                          .defaultValue = 2000,
                          .valueScale = 0.01,
                          .valueFormat = "%.0f",
                          .initiallyEnabled = true,
                          .valueSetter =
                              [](PhysicsSettings& s, double v) {
                                  s.cohesion_resistance_factor = v;
                              },
                          .valueGetter =
                              [](const PhysicsSettings& s) { return s.cohesion_resistance_factor; },
                          .enableSetter = []([[maybe_unused]] PhysicsSettings& s,
                                             [[maybe_unused]] bool e) {},
                          .enableGetter =
                              []([[maybe_unused]] const PhysicsSettings& s) { return true; } },
                        { .label = "Horizontal Flow Resist",
                          .type = ControlType::TOGGLE_SLIDER,
                          .rangeMin = 0,
                          .rangeMax = 2000,
                          .defaultValue = 50,
                          .valueScale = 0.01,
                          .valueFormat = "%.1f",
                          .initiallyEnabled = true,
                          .valueSetter =
                              [](PhysicsSettings& s, double v) {
                                  s.horizontal_flow_resistance_factor = v;
                              },
                          .valueGetter =
                              [](const PhysicsSettings& s) {
                                  return s.horizontal_flow_resistance_factor;
                              },
                          .enableSetter = []([[maybe_unused]] PhysicsSettings& s,
                                             [[maybe_unused]] bool e) {},
                          .enableGetter =
                              []([[maybe_unused]] const PhysicsSettings& s) { return true; } },
                        { .label = "Fluid Lubrication",
                          .type = ControlType::TOGGLE_SLIDER,
                          .rangeMin = 0,
                          .rangeMax = 100,
                          .defaultValue = 50,
                          .valueScale = 0.01,
                          .valueFormat = "%.2f",
                          .initiallyEnabled = true,
                          .valueSetter =
                              [](PhysicsSettings& s, double v) { s.fluid_lubrication_factor = v; },
                          .valueGetter =
                              [](const PhysicsSettings& s) { return s.fluid_lubrication_factor; },
                          .enableSetter = []([[maybe_unused]] PhysicsSettings& s,
                                             [[maybe_unused]] bool e) {},
                          .enableGetter =
                              []([[maybe_unused]] const PhysicsSettings& s) { return true; } } } },
        // Column 5: Swap2 (Advanced swap parameters).
        { .title = "Swap2",
          .controls = { { .label = "Horizontal Non-Fluid Penalty",
                          .type = ControlType::TOGGLE_SLIDER,
                          .rangeMin = 0,
                          .rangeMax = 100,
                          .defaultValue = 10,
                          .valueScale = 0.01,
                          .valueFormat = "%.2f",
                          .initiallyEnabled = true,
                          .valueSetter = [](PhysicsSettings& s,
                                            double v) { s.horizontal_non_fluid_penalty = v; },
                          .valueGetter =
                              [](const PhysicsSettings& s) {
                                  return s.horizontal_non_fluid_penalty;
                              },
                          .enableSetter = []([[maybe_unused]] PhysicsSettings& s,
                                             [[maybe_unused]] bool e) {},
                          .enableGetter =
                              []([[maybe_unused]] const PhysicsSettings& s) { return true; } },
                        { .label = "Horizontal Target Resist",
                          .type = ControlType::TOGGLE_SLIDER,
                          .rangeMin = 0,
                          .rangeMax = 1000,
                          .defaultValue = 200,
                          .valueScale = 0.01,
                          .valueFormat = "%.1f",
                          .initiallyEnabled = true,
                          .valueSetter =
                              [](PhysicsSettings& s, double v) {
                                  s.horizontal_non_fluid_target_resistance = v;
                              },
                          .valueGetter =
                              [](const PhysicsSettings& s) {
                                  return s.horizontal_non_fluid_target_resistance;
                              },
                          .enableSetter = []([[maybe_unused]] PhysicsSettings& s,
                                             [[maybe_unused]] bool e) {},
                          .enableGetter =
                              []([[maybe_unused]] const PhysicsSettings& s) { return true; } },
                        { .label = "Horiz Non-Fluid Energy",
                          .type = ControlType::TOGGLE_SLIDER,
                          .rangeMin = 0,
                          .rangeMax = 10000,
                          .defaultValue = 400,
                          .valueScale = 0.01,
                          .valueFormat = "%.1f",
                          .initiallyEnabled = true,
                          .valueSetter =
                              [](PhysicsSettings& s, double v) {
                                  s.horizontal_non_fluid_energy_multiplier = v;
                              },
                          .valueGetter =
                              [](const PhysicsSettings& s) {
                                  return s.horizontal_non_fluid_energy_multiplier;
                              },
                          .enableSetter = []([[maybe_unused]] PhysicsSettings& s,
                                             [[maybe_unused]] bool e) {},
                          .enableGetter =
                              []([[maybe_unused]] const PhysicsSettings& s) { return true; } } } },
        // Column 6: Frag (Water fragmentation parameters).
        { .title = "Frag",
          .controls = { { .label = "Enabled",
                          .type = ControlType::SWITCH_ONLY,
                          .enableSetter = [](PhysicsSettings& s,
                                             bool e) { s.fragmentation_enabled = e; },
                          .enableGetter =
                              [](const PhysicsSettings& s) { return s.fragmentation_enabled; } },
                        { .label = "Threshold",
                          .type = ControlType::TOGGLE_SLIDER,
                          .rangeMin = 0,
                          .rangeMax = 500,
                          .defaultValue = 50,
                          .valueScale = 0.1,
                          .valueFormat = "%.1f",
                          .initiallyEnabled = true,
                          .valueSetter =
                              [](PhysicsSettings& s, double v) { s.fragmentation_threshold = v; },
                          .valueGetter =
                              [](const PhysicsSettings& s) { return s.fragmentation_threshold; },
                          .enableSetter = []([[maybe_unused]] PhysicsSettings& s,
                                             [[maybe_unused]] bool e) {},
                          .enableGetter =
                              []([[maybe_unused]] const PhysicsSettings& s) { return true; } },
                        { .label = "Full Threshold",
                          .type = ControlType::TOGGLE_SLIDER,
                          .rangeMin = 0,
                          .rangeMax = 1000,
                          .defaultValue = 100,
                          .valueScale = 0.1,
                          .valueFormat = "%.1f",
                          .initiallyEnabled = true,
                          .valueSetter =
                              [](PhysicsSettings& s, double v) {
                                  s.fragmentation_full_threshold = v;
                              },
                          .valueGetter =
                              [](const PhysicsSettings& s) {
                                  return s.fragmentation_full_threshold;
                              },
                          .enableSetter = []([[maybe_unused]] PhysicsSettings& s,
                                             [[maybe_unused]] bool e) {},
                          .enableGetter =
                              []([[maybe_unused]] const PhysicsSettings& s) { return true; } },
                        { .label = "Spray Fraction",
                          .type = ControlType::TOGGLE_SLIDER,
                          .rangeMin = 0,
                          .rangeMax = 100,
                          .defaultValue = 40,
                          .valueScale = 0.01,
                          .valueFormat = "%.2f",
                          .initiallyEnabled = true,
                          .valueSetter =
                              [](PhysicsSettings& s, double v) {
                                  s.fragmentation_spray_fraction = v;
                              },
                          .valueGetter =
                              [](const PhysicsSettings& s) {
                                  return s.fragmentation_spray_fraction;
                              },
                          .enableSetter = []([[maybe_unused]] PhysicsSettings& s,
                                             [[maybe_unused]] bool e) {},
                          .enableGetter =
                              []([[maybe_unused]] const PhysicsSettings& s) { return true; } } } }
    };
}

PhysicsControls::PhysicsControls(lv_obj_t* container, WebSocketClient* wsClient)
    : container_(container), wsClient_(wsClient)
{
    // Create 3-column layout.
    lv_obj_set_flex_flow(container_, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(
        container_, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

    // Create columns and controls from configuration.
    auto configs = createColumnConfigs();

    // Count total controls and reserve space upfront to prevent reallocation.
    size_t totalControls = 0;
    for (const auto& columnConfig : configs) {
        totalControls += columnConfig.controls.size();
    }
    controls_.reserve(totalControls);

    // Create columns and controls in a single pass.
    lv_obj_t* forces_parent_column = nullptr;
    for (const auto& columnConfig : configs) {
        lv_obj_t* column;

        // Special case: Forces, Swap Tuning, and Swap2 share a parent column container.
        const std::string title = columnConfig.title;
        if (title == "Forces") {
            // Create parent column for all swap-related panels (no label - panels have their own).
            forces_parent_column = createColumn("");
            columns_.push_back(forces_parent_column);
            // Create Forces panel inside parent column.
            column = createCollapsibleColumnInContainer(forces_parent_column, columnConfig.title);
        }
        else if (
            (title == "Swap Tuning" || title == "Swap2" || title == "Frag")
            && forces_parent_column) {
            // Create panels inside same parent column.
            column = createCollapsibleColumnInContainer(forces_parent_column, columnConfig.title);
        }
        else {
            // Create new column container with panel.
            column = createCollapsibleColumn(columnConfig.title);
            columns_.push_back(column);
        }

        for (const auto& controlConfig : columnConfig.controls) {
            // Add control to vector first using emplace_back.
            size_t index = controls_.size();
            controls_.emplace_back();
            Control& control = controls_[index];
            control.config = controlConfig;

            // Create the UI widget and map it.
            createControlWidget(column, control);
        }
    }

    // Fetch initial settings from server.
    fetchSettings();

    spdlog::info(
        "PhysicsControls: Initialized with {} controls ({} widgets mapped)",
        controls_.size(),
        widgetToControl_.size());
}

PhysicsControls::~PhysicsControls()
{
    spdlog::info("PhysicsControls: Destroyed");
}

lv_obj_t* PhysicsControls::createColumn(const char* title)
{
    lv_obj_t* column = lv_obj_create(container_);
    lv_obj_set_size(column, LV_PCT(30), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(column, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(column, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(column, 4, 0);
    lv_obj_set_style_pad_all(column, 8, 0);
    lv_obj_set_style_bg_color(column, lv_color_hex(0x303030), 0); // Dark gray background.
    lv_obj_set_style_bg_opa(column, LV_OPA_COVER, 0);

    lv_obj_t* label = lv_label_create(column);
    lv_label_set_text(label, title);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), 0); // White text.

    return column;
}

lv_obj_t* PhysicsControls::createCollapsibleColumn(const char* title)
{
    // Collapse swap-related panels by default.
    const std::string title_str(title);
    const bool should_expand =
        (title_str != "Forces" && title_str != "Swap Tuning" && title_str != "Swap2"
         && title_str != "Frag");

    lv_obj_t* panel = LVGLBuilder::collapsiblePanel(container_)
                          .title(title)
                          .size(LV_PCT(30), LV_SIZE_CONTENT)
                          .initiallyExpanded(should_expand)
                          .backgroundColor(0x303030)
                          .headerColor(0x404040)
                          .buildOrLog();

    if (!panel) {
        spdlog::error("PhysicsControls: Failed to create collapsible panel '{}'", title);
        return nullptr;
    }

    // The content area is the second child (index 1) of the panel.
    // Child 0 is the header, child 1 is the content area.
    lv_obj_t* content = lv_obj_get_child(panel, 1);

    return content;
}

lv_obj_t* PhysicsControls::createCollapsibleColumnInContainer(lv_obj_t* parent, const char* title)
{
    // Collapse swap-related panels by default.
    const std::string title_str(title);
    const bool should_expand =
        (title_str != "Forces" && title_str != "Swap Tuning" && title_str != "Swap2"
         && title_str != "Frag");

    lv_obj_t* panel = LVGLBuilder::collapsiblePanel(parent)
                          .title(title)
                          .size(LV_PCT(100), LV_SIZE_CONTENT)
                          .initiallyExpanded(should_expand)
                          .backgroundColor(0x303030)
                          .headerColor(0x404040)
                          .buildOrLog();

    if (!panel) {
        spdlog::error(
            "PhysicsControls: Failed to create collapsible panel '{}' in container", title);
        return nullptr;
    }

    // The content area is the second child (index 1) of the panel.
    // Child 0 is the header, child 1 is the content area.
    lv_obj_t* content = lv_obj_get_child(panel, 1);

    return content;
}

void PhysicsControls::createControlWidget(lv_obj_t* column, Control& control)
{
    const auto& config = control.config;

    if (config.type == ControlType::TOGGLE_SLIDER) {
        control.widget = LVGLBuilder::toggleSlider(column)
                             .label(config.label)
                             .range(config.rangeMin, config.rangeMax)
                             .value(config.defaultValue)
                             .defaultValue(config.defaultValue)
                             .valueScale(config.valueScale)
                             .valueFormat(config.valueFormat)
                             .initiallyEnabled(config.initiallyEnabled)
                             .sliderWidth(180)
                             .onToggle(onGenericToggle, this)
                             .onSliderChange(onGenericValueChange, this)
                             .buildOrLog();

        if (control.widget) {
            // The ToggleSlider creates a container with children.
            // Child 0: switch, Child 1: label, Child 2: slider, Child 3: value label.
            control.switchWidget = lv_obj_get_child(control.widget, 0);
            control.sliderWidget = lv_obj_get_child(control.widget, 2);

            // Map widgets to control for fast lookup.
            widgetToControl_[control.widget] = &control;
            if (control.switchWidget) {
                widgetToControl_[control.switchWidget] = &control;
            }
            if (control.sliderWidget) {
                widgetToControl_[control.sliderWidget] = &control;
            }
            spdlog::debug(
                "PhysicsControls: Mapped '{}' widgets (container, switch, slider) -> control at "
                "{:p}",
                config.label,
                static_cast<void*>(&control));
        }
    }
    else if (config.type == ControlType::SWITCH_ONLY) {
        control.widget = LVGLBuilder::labeledSwitch(column)
                             .label(config.label)
                             .initialState(config.initiallyEnabled)
                             .callback(onGenericToggle, this)
                             .buildOrLog();

        if (control.widget) {
            control.switchWidget = control.widget;
            widgetToControl_[control.switchWidget] = &control;
        }
    }
}

void PhysicsControls::onGenericToggle(lv_event_t* e)
{
    lv_obj_t* target = static_cast<lv_obj_t*>(lv_event_get_target(e));

    // Get user data - try widget first (for toggleSlider), then event (for labeledSwitch).
    // toggleSlider sets user_data on the widget via lv_obj_set_user_data().
    // labeledSwitch passes user_data through the event callback.
    PhysicsControls* self = static_cast<PhysicsControls*>(lv_obj_get_user_data(target));
    if (!self) {
        // Try event user data as fallback (for labeledSwitch).
        self = static_cast<PhysicsControls*>(lv_event_get_user_data(e));
    }

    if (!self) {
        spdlog::warn(
            "PhysicsControls: onGenericToggle - self is null from both widget and event user_data");
        return;
    }

    PhysicsControls::Control* control = self->findControl(target);
    if (!control) {
        spdlog::warn(
            "PhysicsControls: Could not find control for toggle event (target ptr: {})",
            static_cast<void*>(target));

        // Debug: Log what we do have mapped.
        spdlog::debug("PhysicsControls: Have {} widgets mapped", self->widgetToControl_.size());
        return;
    }

    bool enabled = lv_obj_has_state(target, LV_STATE_CHECKED);
    const char* label = control->config.label ? control->config.label : "Unknown";
    spdlog::info("PhysicsControls: {} toggled to {}", label, enabled ? "ON" : "OFF");

    // Call the enable setter if it exists.
    if (control->config.enableSetter) {
        try {
            control->config.enableSetter(self->settings_, enabled);
        }
        catch (const std::exception& ex) {
            spdlog::error(
                "PhysicsControls: Exception in enableSetter for {}: {}", label, ex.what());
            return;
        }
    }

    // For toggle sliders, when re-enabled, restore the slider value.
    if (enabled && control->config.type == ControlType::TOGGLE_SLIDER) {
        // The slider is a sibling of the toggle switch, not in parent container.
        // We need to use the stored sliderWidget pointer.
        if (control->sliderWidget) {
            int value = lv_slider_get_value(control->sliderWidget);
            double scaledValue = value * control->config.valueScale;
            if (control->config.valueSetter) {
                control->config.valueSetter(self->settings_, scaledValue);
            }
            spdlog::debug(
                "PhysicsControls: Restored {} to {:.2f}", control->config.label, scaledValue);
        }
        else {
            spdlog::warn("PhysicsControls: No slider widget found for {}", control->config.label);
        }
    }

    try {
        self->syncSettings();
    }
    catch (const std::exception& ex) {
        spdlog::error("PhysicsControls: Exception in syncSettings: {}", ex.what());
    }
}

void PhysicsControls::onGenericValueChange(lv_event_t* e)
{
    lv_obj_t* target = static_cast<lv_obj_t*>(lv_event_get_target(e));

    // Get user data - try widget first (for toggleSlider), then event as fallback.
    // toggleSlider sets user_data on the slider widget via lv_obj_set_user_data().
    PhysicsControls* self = static_cast<PhysicsControls*>(lv_obj_get_user_data(target));
    if (!self) {
        // Try event user data as fallback.
        self = static_cast<PhysicsControls*>(lv_event_get_user_data(e));
    }

    if (!self) {
        spdlog::warn("PhysicsControls: onGenericValueChange - self is null from both widget and "
                     "event user_data");
        return;
    }

    PhysicsControls::Control* control = self->findControl(target);
    if (!control) {
        spdlog::warn("PhysicsControls: Could not find control for value change event");
        return;
    }

    int value = lv_slider_get_value(target);
    double scaledValue = value * control->config.valueScale;

    spdlog::info("PhysicsControls: {} changed to {:.2f}", control->config.label, scaledValue);

    // Call the value setter if it exists.
    if (control->config.valueSetter) {
        control->config.valueSetter(self->settings_, scaledValue);
    }

    self->syncSettings();
}

PhysicsControls::Control* PhysicsControls::findControl(lv_obj_t* widget)
{
    // Sanity check: the map should have a reasonable number of entries.
    size_t mapSize = widgetToControl_.size();
    if (mapSize > 1000) {
        spdlog::error(
            "PhysicsControls: findControl - map corrupted! Size={}, this={:p}",
            mapSize,
            static_cast<void*>(this));
        spdlog::error("PhysicsControls: This object may have been moved or is invalid");
        return nullptr;
    }

    // Direct lookup.
    auto it = widgetToControl_.find(widget);
    if (it != widgetToControl_.end()) {
        spdlog::debug(
            "PhysicsControls: Found control '{}' for widget {:p}",
            it->second->config.label,
            static_cast<void*>(widget));
        return it->second;
    }

    // If not found, check if this widget's parent is in our map.
    // This handles cases where the event target is a child of the mapped widget.
    lv_obj_t* parent = lv_obj_get_parent(widget);
    if (parent) {
        it = widgetToControl_.find(parent);
        if (it != widgetToControl_.end()) {
            return it->second;
        }

        // Check grandparent too (for deeply nested widgets).
        lv_obj_t* grandparent = lv_obj_get_parent(parent);
        if (grandparent) {
            it = widgetToControl_.find(grandparent);
            if (it != widgetToControl_.end()) {
                return it->second;
            }
        }
    }

    return nullptr;
}

void PhysicsControls::updateFromSettings(const PhysicsSettings& settings)
{
    spdlog::info("PhysicsControls: Updating UI from server settings");

    // Update local settings copy.
    settings_ = settings;

    // Helper to update a toggle slider control.
    auto updateToggleSlider = [](PhysicsControls::Control* control, double value, bool enabled) {
        if (!control || !control->widget) return;

        // For toggle sliders, update both switch and slider.
        if (control->config.type == ControlType::TOGGLE_SLIDER) {
            // Control is a container with toggle (child 0), label (child 1), slider (child 2),
            // valueLabel (child 3).
            lv_obj_t* toggle = lv_obj_get_child(control->widget, 0);
            lv_obj_t* slider = lv_obj_get_child(control->widget, 2);
            lv_obj_t* valueLabel = lv_obj_get_child(control->widget, 3);

            if (toggle) {
                if (enabled) {
                    lv_obj_add_state(toggle, LV_STATE_CHECKED);
                }
                else {
                    lv_obj_remove_state(toggle, LV_STATE_CHECKED);
                }
            }

            if (slider) {
                // Convert double value back to integer slider value.
                int sliderValue = static_cast<int>(value / control->config.valueScale);
                lv_slider_set_value(slider, sliderValue, LV_ANIM_OFF);

                // Manually update value label (lv_slider_set_value doesn't trigger events).
                if (valueLabel) {
                    char buf[32];
                    snprintf(buf, sizeof(buf), control->config.valueFormat, value);
                    lv_label_set_text(valueLabel, buf);
                }
            }
        }
        else if (control->config.type == ControlType::SWITCH_ONLY) {
            // For switch-only controls.
            if (control->switchWidget) {
                if (enabled) {
                    lv_obj_add_state(control->switchWidget, LV_STATE_CHECKED);
                }
                else {
                    lv_obj_remove_state(control->switchWidget, LV_STATE_CHECKED);
                }
            }
        }
    };

    // Update all controls from settings.
    for (auto& control : controls_) {
        if (control.config.valueGetter && control.config.enableGetter) {
            double value = control.config.valueGetter(settings);
            bool enabled = control.config.enableGetter(settings);
            updateToggleSlider(&control, value, enabled);
        }
        else if (control.config.enableGetter) {
            // Switch-only control.
            bool enabled = control.config.enableGetter(settings);
            updateToggleSlider(&control, 0.0, enabled);
        }
    }

    spdlog::info("PhysicsControls: UI updated from server settings");
}

void PhysicsControls::fetchSettings()
{
    if (!wsClient_ || !wsClient_->isConnected()) {
        spdlog::warn("PhysicsControls: Cannot fetch settings - not connected");
        return;
    }

    spdlog::info("PhysicsControls: Fetching physics settings from server");

    const Api::PhysicsSettingsGet::Command cmd{};
    wsClient_->sendCommand(cmd);

    // Response will be handled by UI state machine and used to update controls.
}

void PhysicsControls::syncSettings()
{
    if (!wsClient_ || !wsClient_->isConnected()) {
        spdlog::warn("PhysicsControls: Cannot sync settings - not connected");
        return;
    }

    spdlog::debug("PhysicsControls: Syncing physics settings to server");

    const Api::PhysicsSettingsSet::Command cmd{ .settings = settings_ };
    wsClient_->sendCommand(cmd);
}

} // namespace Ui
} // namespace DirtSim
