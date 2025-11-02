#include "SimulatorUI.h"
#include "Cell.h"
#include "Event.h"
#include "EventRouter.h"
#include "MaterialType.h"
#include "SharedSimState.h"
#include "SparkleAssert.h"
#include "UIUpdateConsumer.h"
#include "World.h"

#include "lvgl/lvgl.h"
#include "lvgl/src/misc/lv_timer.h"
#include "lvgl/src/others/snapshot/lv_snapshot.h"
#include "scenarios/Scenario.h"
#include "scenarios/ScenarioRegistry.h"
#include "scenarios/ScenarioWorldEventGenerator.h"
#include "spdlog/spdlog.h"
#include "ui/LVGLBuilder.h"
#include "ui/LVGLEventBuilder.h"

#include "Event.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <libgen.h> // For dirname.
#include <limits.h> // For PATH_MAX.
#include <stdexcept>
#include <unistd.h> // For readlink on Linux.
#include <vector>

using namespace DirtSim;

// Forward declare lodepng functions to avoid header conflicts.
extern "C" {
unsigned lodepng_encode24(
    unsigned char** out, size_t* outsize, const unsigned char* image, unsigned w, unsigned h);
const char* lodepng_error_text(unsigned error);
}

SimulatorUI::SimulatorUI(lv_obj_t* screen, EventRouter* eventRouter)
    : event_router_(eventRouter),
      screen_(screen),
      draw_area_(nullptr),
      mass_label_(nullptr),
      fps_label_(nullptr),
      pause_label_(nullptr),
      timescale_(1.0),
      is_paused_(false),
      frame_limiting_enabled_(true), // Default to frame limiting enabled.
      interaction_mode_(InteractionMode::NONE),
      paint_material_(MaterialType::DIRT)
{
    // Pre-reserve capacity for callback data to prevent reallocation
    callback_data_storage_.reserve(200);
}

SimulatorUI::CallbackData* SimulatorUI::createCallbackData(lv_obj_t* label)
{
    auto data = std::make_unique<CallbackData>();
    data->ui = this;
    data->associated_label = label;

    // Reserve capacity to prevent reallocation if we're getting close
    if (callback_data_storage_.size() >= callback_data_storage_.capacity() - 10) {
        callback_data_storage_.reserve(callback_data_storage_.capacity() + 100);
    }

    CallbackData* ptr = data.get();
    callback_data_storage_.push_back(std::move(data));
    return ptr;
}

void SimulatorUI::uiUpdateTimerCb(lv_timer_t* timer)
{
    SimulatorUI* ui = static_cast<SimulatorUI*>(lv_timer_get_user_data(timer));
    if (ui && ui->updateConsumer_) {
        ui->updateConsumer_->consumeUpdate();
    }
}

SimulatorUI::~SimulatorUI()
{
    // Clean up LVGL timer.
    if (updateTimer_) {
        lv_timer_delete(updateTimer_);
        updateTimer_ = nullptr;
    }
}

void SimulatorUI::initialize()
{
    // Verify LVGL is initialized.
    if (!lv_is_initialized()) {
        spdlog::error("SimulatorUI::initialize() - LVGL is not initialized! Call lv_init() first.");
        throw std::runtime_error("LVGL must be initialized before creating SimulatorUI");
    }

    // Verify we have a display.
    if (lv_display_get_default() == nullptr) {
        spdlog::error("SimulatorUI::initialize() - No LVGL display found! Create a display before "
                      "initializing UI.");
        throw std::runtime_error("LVGL requires a display to be created before UI initialization. "
                                 "Use lv_display_create() or one of the display backends.");
    }

    // Verify screen is valid.
    if (!screen_) {
        spdlog::error("SimulatorUI::initialize() - Invalid screen pointer!");
        throw std::runtime_error("SimulatorUI requires a valid screen object");
    }

    // Set black background for the main screen.
    lv_obj_set_style_bg_color(screen_, lv_color_hex(0x000000), 0);

    createDrawArea();
    createLabels();
    createScenarioDropdown();
    createMaterialPicker();
    createControlButtons();
    createSliders();

    // Initialize push-based UI update system (always enabled for thread safety).
    if (event_router_) {
        SharedSimState& sharedState = event_router_->getSharedSimState();

        // Create UIUpdateConsumer.
        updateConsumer_ = std::make_unique<UIUpdateConsumer>(&sharedState, this);

        // Create LVGL timer for 60fps updates (16.67ms).
        updateTimer_ = lv_timer_create(uiUpdateTimerCb, 16, this);

        spdlog::info("Push-based UI update system initialized with 60fps timer");
    }
}

void SimulatorUI::createDrawArea()
{
    draw_area_ = LVGLEventBuilder::drawArea(screen_, event_router_)
                     .size(DRAW_AREA_SIZE, DRAW_AREA_SIZE)
                     .position(0, 0, LV_ALIGN_LEFT_MID)
                     .onMouseEvents() // This sets up mouse down/move/up events.
                     .buildOrLog();
    if (draw_area_) {
        lv_obj_set_style_pad_all(draw_area_, 0, 0);
    }
}

void SimulatorUI::createLabels()
{
    // Create mass label.
    mass_label_ = lv_label_create(screen_);
    lv_label_set_text(mass_label_, "Total Mass: 0.00");
    lv_obj_set_style_text_color(mass_label_, lv_color_hex(0xFFFFFF), 0);  // White text.
    lv_obj_align(mass_label_, LV_ALIGN_TOP_LEFT, RIGHT_COLUMN_X, 10);

    // Create FPS label - positioned over the world area.
    fps_label_ = lv_label_create(screen_);
    lv_label_set_text(fps_label_, "FPS: 0");
    lv_obj_set_style_text_color(fps_label_, lv_color_hex(0xFFFFFF), 0);  // White text.
    lv_obj_align(fps_label_, LV_ALIGN_TOP_LEFT, 10, 10); // Top-left corner of world area.

    // Create frame limiting toggle button below FPS display.
    LVGLEventBuilder::button(screen_, event_router_)
        .onFrameLimitToggle()
        .size(120, 30)
        .position(10, 40, LV_ALIGN_TOP_LEFT)
        .text("Limit: On")
        .buildOrLog();
}

void SimulatorUI::createScenarioDropdown()
{
    // Scenario label.
    lv_obj_t* scenario_label = lv_label_create(screen_);
    lv_label_set_text(scenario_label, "Scenario:");
    lv_obj_set_style_text_color(scenario_label, lv_color_hex(0xFFFFFF), 0);  // White text.
    lv_obj_set_style_bg_opa(scenario_label, LV_OPA_TRANSP, 0);  // Transparent background.
    lv_obj_align(scenario_label, LV_ALIGN_TOP_LEFT, LEFT_COLUMN_X, 10);

    // Scenario dropdown.
    scenario_dropdown_ = lv_dropdown_create(screen_);
    lv_obj_set_size(scenario_dropdown_, CONTROL_WIDTH, 30);
    lv_obj_align(scenario_dropdown_, LV_ALIGN_TOP_LEFT, LEFT_COLUMN_X, 30);

    // Dark mode styling for dropdown.
    lv_obj_set_style_bg_color(scenario_dropdown_, lv_color_hex(0x404040), 0);  // Dark background.
    lv_obj_set_style_text_color(scenario_dropdown_, lv_color_hex(0xFFFFFF), 0);  // White text.

    // Style the dropdown list (the part that opens).
    lv_obj_t* list = lv_dropdown_get_list(scenario_dropdown_);
    if (list) {
        lv_obj_set_style_bg_color(list, lv_color_hex(0x404040), 0);  // Dark background.
        lv_obj_set_style_text_color(list, lv_color_hex(0xFFFFFF), 0);  // White text.
    }

    // TODO: Populate dropdown with scenarios from registry.
    // updateScenarioDropdown();

    // TODO: Set event callback for scenario selection.
    // lv_obj_add_event_cb(
    //     scenario_dropdown_, onScenarioChanged, LV_EVENT_VALUE_CHANGED, createCallbackData());
}

void SimulatorUI::createMaterialPicker()
{
    // Create material picker label.
    lv_obj_t* material_label = lv_label_create(screen_);
    lv_label_set_text(material_label, "Materials:");
    lv_obj_set_style_text_color(material_label, lv_color_hex(0xFFFFFF), 0);  // White text.
    lv_obj_set_style_bg_opa(material_label, LV_OPA_TRANSP, 0);  // Transparent background.
    lv_obj_align(
        material_label, LV_ALIGN_TOP_LEFT, LEFT_COLUMN_X, 70); // Below scenario dropdown.

    // Create material picker container.
    lv_obj_t* picker_container = lv_obj_create(screen_);
    lv_obj_set_size(
        picker_container, CONTROL_WIDTH, 320); // Give enough space for 4x2 grid.
    lv_obj_align(picker_container, LV_ALIGN_TOP_LEFT, LEFT_COLUMN_X, 90); // Below label.
    lv_obj_set_style_pad_all(picker_container, 5, 0);
    lv_obj_set_style_border_width(picker_container, 1, 0);
    lv_obj_set_style_border_color(picker_container, lv_color_hex(0x606060), 0);
    lv_obj_set_style_bg_color(picker_container, lv_color_hex(0x000000), 0);  // Black background.

    // Disable scrollbars.
    lv_obj_clear_flag(picker_container, LV_OBJ_FLAG_SCROLLABLE);

    // Create MaterialPicker instance with event router.
    material_picker_ = std::make_unique<MaterialPicker>(picker_container, event_router_);
    material_picker_->createMaterialSelector();

    spdlog::info("Material picker created in SimulatorUI");
}

void SimulatorUI::createControlButtons()
{
    // Create debug toggle button.
    if (event_router_) {
        debug_btn_ = LVGLEventBuilder::button(screen_, event_router_)
                         .onDebugToggle()
                         .size(CONTROL_WIDTH, 50)
                         .position(RIGHT_COLUMN_X, 10)
                         .text("Debug: Off")
                         .buildOrLog();
    }

    // === WorldA Pressure Controls ===.
    lv_obj_t* worldA_pressure_header = lv_label_create(screen_);
    lv_label_set_text(worldA_pressure_header, "=== WorldA Pressure ===");
    lv_obj_set_style_text_color(worldA_pressure_header, lv_color_hex(0xFFFFFF), 0);  // White text.
    lv_obj_align(worldA_pressure_header, LV_ALIGN_TOP_LEFT, RIGHT_COLUMN_X, 70);

    // Create pressure system dropdown.
    lv_obj_t* pressure_label = lv_label_create(screen_);
    lv_label_set_text(pressure_label, "System:");
    lv_obj_set_style_text_color(pressure_label, lv_color_hex(0xFFFFFF), 0);  // White text.
    lv_obj_set_style_bg_opa(pressure_label, LV_OPA_TRANSP, 0);  // Transparent background.
    lv_obj_align(pressure_label, LV_ALIGN_TOP_LEFT, RIGHT_COLUMN_X, 95);

    lv_obj_t* pressure_dropdown = LVGLEventBuilder::dropdown(screen_, event_router_)
                                       .onPressureSystemChange()
                                       .size(CONTROL_WIDTH, 40)
                                       .position(RIGHT_COLUMN_X, 115, LV_ALIGN_TOP_LEFT)
                                       .options("Original (COM)\nTop-Down Hydrostatic\nIterative Settling")
                                       .selected(0)
                                       .buildOrLog();

    // Style the pressure dropdown to match blue buttons.
    if (pressure_dropdown) {
        lv_obj_set_style_bg_color(pressure_dropdown, lv_color_hex(0x0080FF), 0);  // Blue background.
        lv_obj_set_style_text_color(pressure_dropdown, lv_color_hex(0xFFFFFF), 0);  // White text.

        // Style the dropdown list (the part that opens).
        lv_obj_t* list = lv_dropdown_get_list(pressure_dropdown);
        if (list) {
            lv_obj_set_style_bg_color(list, lv_color_hex(0x404040), 0);  // Dark background.
            lv_obj_set_style_text_color(list, lv_color_hex(0xFFFFFF), 0);  // White text.
        }
    }

    // Pressure scale slider (WorldA only).
    LVGLEventBuilder::slider(screen_, event_router_)
        .onPressureScaleChange()
        .position(RIGHT_COLUMN_X, 185, LV_ALIGN_TOP_LEFT)
        .size(CONTROL_WIDTH, 10)
        .range(0, 1000)
        .value(100)
        .label("Strength", 0, -20)
        .valueLabel("%.1f", 135, -20)
        .buildOrLog();

    // Create gravity slider (-10x to +10x Earth gravity).
    LVGLEventBuilder::slider(screen_, event_router_)
        .onGravityChange()
        .position(RIGHT_COLUMN_X, 245, LV_ALIGN_TOP_LEFT)
        .size(CONTROL_WIDTH, 10)
        .range(-1000, 1000) // -10x to +10x.
        .value(100)         // 1x Earth gravity (9.81).
        .label("Gravity", 0, -20)
        .valueLabel("%.1f", 80, -20)
        .buildOrLog();

    // Create viscosity strength slider.
    LVGLEventBuilder::slider(screen_, event_router_)
        .onViscosityStrengthChange()
        .position(RIGHT_COLUMN_X, 285, LV_ALIGN_TOP_LEFT)
        .size(CONTROL_WIDTH, 10)
        .range(0, 200)
        .value(100)
        .label("Viscosity", 0, -20)
        .valueLabel("%.1f", 80, -20)
        .buildOrLog();

    // Create cohesion force toggle slider (integrated switch + slider).
    cohesion_switch_ = LVGLEventBuilder::toggleSlider(screen_, event_router_)
                           .label("Cohesion Force")
                           .position(RIGHT_COLUMN_X, 320, LV_ALIGN_TOP_LEFT)
                           .sliderWidth(CONTROL_WIDTH)
                           .range(0, 30000)
                           .value(15000)
                           .defaultValue(15000)
                           .valueScale(0.01)
                           .valueFormat("%.1f")
                           .valueLabelOffset(165, -20)
                           .initiallyEnabled(false)
                           .onValueChange([](double value) {
                               return Event{SetCohesionForceStrengthCommand{value}};
                           })
                           .buildOrLog();

    // Create COM cohesion range slider.
    LVGLEventBuilder::slider(screen_, event_router_)
        .onCOMCohesionRangeChange()
        .position(RIGHT_COLUMN_X, 405, LV_ALIGN_TOP_LEFT)
        .size(CONTROL_WIDTH, 10)
        .range(1, 5)
        .value(1)
        .label("Cohesion Range", 0, -20)
        .valueLabel("%.0f", 120, -20)
        .buildOrLog();

    // Create friction toggle slider (velocity-dependent viscosity).
    LVGLEventBuilder::toggleSlider(screen_, event_router_)
        .label("Friction")
        .position(RIGHT_COLUMN_X, 440, LV_ALIGN_TOP_LEFT)
        .sliderWidth(CONTROL_WIDTH)
        .range(0, 1000)
        .value(100)
        .defaultValue(100)
        .valueScale(0.01)
        .valueFormat("%.1f")
        .initiallyEnabled(true)
        .onValueChange([](double value) {
            return Event{SetFrictionStrengthCommand{value}};
        })
        .buildOrLog();

    // Create contact friction toggle slider (surface-to-surface friction).
    LVGLEventBuilder::toggleSlider(screen_, event_router_)
        .label("Contact")
        .position(RIGHT_COLUMN_X, 520, LV_ALIGN_TOP_LEFT)
        .sliderWidth(CONTROL_WIDTH)
        .range(0, 1000)
        .value(100)
        .defaultValue(100)
        .valueScale(0.01)
        .valueFormat("%.1f")
        .initiallyEnabled(true)
        .onValueChange([](double value) {
            return Event{SetContactFrictionStrengthCommand{value}};
        })
        .buildOrLog();

    // Create adhesion toggle slider (integrated switch + slider).
    adhesion_switch_ = LVGLEventBuilder::toggleSlider(screen_, event_router_)
                           .label("Adhesion")
                           .position(RIGHT_COLUMN_X, 600, LV_ALIGN_TOP_LEFT)
                           .sliderWidth(CONTROL_WIDTH)
                           .range(0, 1000)
                           .value(500)
                           .defaultValue(500)
                           .valueScale(0.01)
                           .valueFormat("%.1f")
                           .valueLabelOffset(140, -20)
                           .initiallyEnabled(false)
                           .onValueChange([](double value) {
                               return Event{SetAdhesionStrengthCommand{value}};
                           })
                           .buildOrLog();

    // Create quadrant toggle.
    LVGLEventBuilder::labeledSwitch(screen_, event_router_)
        .label("Quadrant")
        .position(RIGHT_COLUMN_X, 680, LV_ALIGN_TOP_LEFT)
        .onQuadrantToggle()
        .checked(true)
        .buildOrLog();

    // Create water column toggle.
    LVGLEventBuilder::labeledSwitch(screen_, event_router_)
        .label("Water Column")
        .position(RIGHT_COLUMN_X, 710, LV_ALIGN_TOP_LEFT)
        .onWaterColumnToggle()
        .checked(true)
        .buildOrLog();

    // Create left throw toggle.
    LVGLEventBuilder::labeledSwitch(screen_, event_router_)
        .label("Left Throw")
        .position(RIGHT_COLUMN_X, 740, LV_ALIGN_TOP_LEFT)
        .onLeftThrowToggle()
        .checked(false)
        .buildOrLog();

    // Create right throw toggle.
    LVGLEventBuilder::labeledSwitch(screen_, event_router_)
        .label("Right Throw")
        .position(RIGHT_COLUMN_X, 770, LV_ALIGN_TOP_LEFT)
        .onRightThrowToggle()
        .checked(true)
        .buildOrLog();

    // Create screenshot button.
    LVGLEventBuilder::button(screen_, event_router_)
        .onScreenshot() // Call event method first.
        .size(CONTROL_WIDTH, 50)
        .position(RIGHT_COLUMN_X, 800, LV_ALIGN_TOP_LEFT)
        .text("Screenshot")
        .buildOrLog();

    // Create print ASCII button.
    LVGLEventBuilder::button(screen_, event_router_)
        .onPrintAscii()
        .size(CONTROL_WIDTH, 50)
        .position(RIGHT_COLUMN_X, 860, LV_ALIGN_TOP_LEFT)
        .text("Print ASCII")
        .buildOrLog();

    // Create spawn ball button.
    LVGLEventBuilder::button(screen_, event_router_)
        .onSpawnDirtBall()
        .size(CONTROL_WIDTH, 50)
        .position(RIGHT_COLUMN_X, 920, LV_ALIGN_TOP_LEFT)
        .text("Spawn ball")
        .buildOrLog();

    // Time reversal controls have been moved to slider column.

    // Create quit button.
    auto quit_btn = LVGLEventBuilder::button(screen_, event_router_)
                        .onQuit()
                        .size(CONTROL_WIDTH, 50)
                        .position(-10, -10, LV_ALIGN_BOTTOM_RIGHT)
                        .text("Quit")
                        .buildOrLog();
    if (quit_btn) {
        lv_obj_set_style_bg_color(quit_btn, lv_color_hex(0xFF0000), 0);
    }
}

void SimulatorUI::createSliders()
{
    // Position sliders to the right of the main control buttons.
    const int SLIDER_COLUMN_X = RIGHT_COLUMN_X + CONTROL_WIDTH + 10;

    // Move Pause/Resume button to top of slider column.
    pause_btn_ = LVGLEventBuilder::button(screen_, event_router_)
                     .onPauseResume() // Call event method first.
                     .size(CONTROL_WIDTH, 50)
                     .position(SLIDER_COLUMN_X, 10, LV_ALIGN_TOP_LEFT)
                     .buildOrLog();
    if (pause_btn_) {
        pause_label_ = lv_label_create(pause_btn_);
        lv_label_set_text(pause_label_, "Pause");
        lv_obj_center(pause_label_);
    }

    // Move Reset button below Pause.
    LVGLEventBuilder::button(screen_, event_router_)
        .onReset() // Call event method first.
        .size(CONTROL_WIDTH, 50)
        .position(SLIDER_COLUMN_X, 70, LV_ALIGN_TOP_LEFT)
        .text("Reset")
        .buildOrLog();

    // Move Time History controls below Reset.
    LVGLEventBuilder::button(screen_, event_router_)
        .onTimeHistoryToggle()
        .size(CONTROL_WIDTH, 30)
        .position(SLIDER_COLUMN_X, 130, LV_ALIGN_TOP_LEFT)
        .text("Time History: On")
        .buildOrLog();

    // Backward and Forward buttons below Time History.
    LVGLEventBuilder::button(screen_, event_router_)
        .onStepBackward()
        .size(CONTROL_WIDTH / 2 - 5, 30)
        .position(SLIDER_COLUMN_X, 165, LV_ALIGN_TOP_LEFT)
        .text("<<")
        .buildOrLog();

    LVGLEventBuilder::button(screen_, event_router_)
        .onStepForward()
        .size(CONTROL_WIDTH / 2 - 5, 30)
        .position(SLIDER_COLUMN_X + CONTROL_WIDTH / 2 + 5, 165, LV_ALIGN_TOP_LEFT)
        .text(">>")
        .buildOrLog();

    // Start sliders below the moved buttons.
    // Timescale slider.
    auto timescale_slider = LVGLEventBuilder::slider(screen_, event_router_)
                                .onTimescaleChange() // Call event method first.
                                .position(SLIDER_COLUMN_X, 230, LV_ALIGN_TOP_LEFT)
                                .size(CONTROL_WIDTH, 10)
                                .range(0, 100)
                                .value(50)
                                .label("Timescale", 0, -20)
                                .valueLabel("%.1fx", 110, -20);
    timescale_slider.buildOrLog();
    // Store the slider and value label references.
    timescale_slider_ = timescale_slider.getSlider();
    timescale_label_ = timescale_slider.getValueLabel();

    // Elasticity slider - migrated to EventRouter.
    auto elasticity_slider = LVGLEventBuilder::slider(screen_, event_router_)
                                 .onElasticityChange() // Call event method first.
                                 .position(SLIDER_COLUMN_X, 270, LV_ALIGN_TOP_LEFT)
                                 .size(CONTROL_WIDTH, 10)
                                 .range(0, 200)
                                 .value(80)
                                 .label("Elasticity")
                                 .valueLabel("%.1f");
    elasticity_slider.buildOrLog();
    // Store the value label reference.
    elasticity_label_ = elasticity_slider.getValueLabel();

    // Dirt fragmentation slider.
    LVGLEventBuilder::slider(screen_, event_router_)
        .onFragmentationChange()
        .position(SLIDER_COLUMN_X, 310, LV_ALIGN_TOP_LEFT)
        .size(CONTROL_WIDTH, 10)
        .range(0, 100)
        .value(0)
        .label("Dirt Fragmentation", 0, -20)
        .valueLabel("%.2f", 155, -20)
        .buildOrLog();

    // Cell size slider.
    spdlog::info("Creating cell size slider - 30 returns: {}", 30);
    LVGLEventBuilder::slider(screen_, event_router_)
        .onCellSizeChange()
        .position(SLIDER_COLUMN_X, 350, LV_ALIGN_TOP_LEFT)
        .size(CONTROL_WIDTH, 10)
        .range(10, 100)
        .value(30)
        .label("Cell Size", 0, -20)
        .valueLabel("%.0f", 110, -20)
        .buildOrLog();

    // Rain rate slider.
    LVGLEventBuilder::slider(screen_, event_router_)
        .onRainRateChange()
        .position(SLIDER_COLUMN_X, 430, LV_ALIGN_TOP_LEFT)
        .size(CONTROL_WIDTH, 10)
        .range(0, 100)
        .value(0)
        .label("Rain Rate", 0, -20)
        .valueLabel("%d/s", 110, -20)
        .buildOrLog();

    // Water cohesion slider.
    LVGLEventBuilder::slider(screen_, event_router_)
        .onWaterCohesionChange()
        .position(SLIDER_COLUMN_X, 470, LV_ALIGN_TOP_LEFT)
        .size(CONTROL_WIDTH, 10)
        .range(0, 1000)
        .value(600)
        .label("Water Cohesion", 0, -20)
        .valueLabel("%.3f", 150, -20)
        .buildOrLog();

    // Water viscosity slider.
    LVGLEventBuilder::slider(screen_, event_router_)
        .onWaterViscosityChange()
        .position(SLIDER_COLUMN_X, 510, LV_ALIGN_TOP_LEFT)
        .size(CONTROL_WIDTH, 10)
        .range(0, 1000)
        .value(100)
        .label("Water Viscosity", 0, -20)
        .valueLabel("%.3f", 150, -20)
        .buildOrLog();

    // Water pressure threshold slider.
    LVGLEventBuilder::slider(screen_, event_router_)
        .onWaterPressureThresholdChange()
        .position(SLIDER_COLUMN_X, 550, LV_ALIGN_TOP_LEFT)
        .size(CONTROL_WIDTH, 10)
        .range(0, 1000)
        .value(40)
        .label("Water Pressure Threshold", 0, -20)
        .valueLabel("%.4f", 190, -20)
        .buildOrLog();

    // Water buoyancy slider.
    LVGLEventBuilder::slider(screen_, event_router_)
        .onWaterBuoyancyChange()
        .position(SLIDER_COLUMN_X, 590, LV_ALIGN_TOP_LEFT)
        .size(CONTROL_WIDTH, 10)
        .range(0, 1000)
        .value(100)
        .label("Water Buoyancy", 0, -20)
        .valueLabel("%.3f", 150, -20)
        .buildOrLog();

    // === World Pressure Controls ===.
    lv_obj_t* worldB_pressure_header = lv_label_create(screen_);
    lv_label_set_text(worldB_pressure_header, "=== World Pressure ===");
    lv_obj_set_style_text_color(worldB_pressure_header, lv_color_hex(0xFFFFFF), 0);  // White text.
    lv_obj_align(worldB_pressure_header, LV_ALIGN_TOP_LEFT, SLIDER_COLUMN_X, 620);

    // Hydrostatic pressure toggle slider (integrated switch + slider).
    hydrostatic_switch_ = LVGLEventBuilder::toggleSlider(screen_, event_router_)
                              .label("Hydrostatic Pressure")
                              .position(SLIDER_COLUMN_X, 645, LV_ALIGN_TOP_LEFT)
                              .sliderWidth(CONTROL_WIDTH)
                              .range(0, 300)
                              .value(100)
                              .defaultValue(100)
                              .valueScale(0.01)
                              .valueFormat("%.2f")
                              .initiallyEnabled(false)
                              .onValueChange([](double value) {
                                  return Event{SetHydrostaticPressureStrengthCommand{value}};
                              })
                              .buildOrLog();

    // Dynamic pressure toggle slider (integrated switch + slider).
    dynamic_switch_ = LVGLEventBuilder::toggleSlider(screen_, event_router_)
                          .label("Dynamic Pressure")
                          .position(SLIDER_COLUMN_X, 725, LV_ALIGN_TOP_LEFT)
                          .sliderWidth(CONTROL_WIDTH)
                          .range(0, 300)
                          .value(100)
                          .defaultValue(100)
                          .valueScale(0.01)
                          .valueFormat("%.2f")
                          .initiallyEnabled(false)
                          .onValueChange([](double value) {
                              return Event{SetDynamicPressureStrengthCommand{value}};
                          })
                          .buildOrLog();

    // Pressure diffusion toggle.
    diffusion_switch_ = LVGLEventBuilder::labeledSwitch(screen_, event_router_)
                            .label("Pressure Diffusion")
                            .position(SLIDER_COLUMN_X, 805, LV_ALIGN_TOP_LEFT)
                            .switchOffset(145)
                            .onPressureDiffusionToggle()
                            .checked(false)
                            .buildOrLog();

    // Air resistance slider.
    LVGLEventBuilder::slider(screen_, event_router_)
        .onAirResistanceChange()
        .position(SLIDER_COLUMN_X, 855, LV_ALIGN_TOP_LEFT)
        .size(CONTROL_WIDTH, 10)
        .range(0, 100)
        .value(10)
        .label("Air Resistance", 0, -20)
        .valueLabel("%.2f", 120, -20)
        .buildOrLog();

    // Pressure scale slider for World.
    LVGLEventBuilder::slider(screen_, event_router_)
        .onPressureScaleWorldBChange()
        .position(SLIDER_COLUMN_X, 915, LV_ALIGN_TOP_LEFT)
        .size(CONTROL_WIDTH, 10)
        .range(0, 200)
        .value(100)
        .label("Pressure Scale", 0, -20)
        .valueLabel("%.1f", 120, -20)
        .buildOrLog();
}

void SimulatorUI::updateMassLabel(double totalMass)
{
    if (mass_label_) {
        char buf[32];
        snprintf(buf, sizeof(buf), "Total Mass: %.2f", totalMass);
        lv_label_set_text(mass_label_, buf);
    }
}

void SimulatorUI::populateFromUpdate(const UIUpdateEvent& update)
{
    spdlog::info("Populating UI controls from update event");

    // Store world state for future comparisons.
    lastWorldState_.emplace(update.world);

    // Update labels.
    updateMassLabel(update.world.getTotalMass());

    // Update world type button matrix

    // Update material selection
    if (material_picker_) {
        material_picker_->setSelectedMaterial(update.world.getSelectedMaterial());
    }

    // Update pressure control switches.
    if (hydrostatic_switch_) {
        bool enabled = update.world.isHydrostaticPressureEnabled();
        if (enabled) {
            lv_obj_add_state(hydrostatic_switch_, LV_STATE_CHECKED);
        }
        else {
            lv_obj_clear_state(hydrostatic_switch_, LV_STATE_CHECKED);
        }
    }

    if (dynamic_switch_) {
        bool enabled = update.world.isDynamicPressureEnabled();
        if (enabled) {
            lv_obj_add_state(dynamic_switch_, LV_STATE_CHECKED);
        }
        else {
            lv_obj_clear_state(dynamic_switch_, LV_STATE_CHECKED);
        }
    }

    if (diffusion_switch_) {
        bool enabled = update.world.isPressureDiffusionEnabled();
        if (enabled) {
            lv_obj_add_state(diffusion_switch_, LV_STATE_CHECKED);
        }
        else {
            lv_obj_clear_state(diffusion_switch_, LV_STATE_CHECKED);
        }
    }

    // Update sliders based on world properties
    

    // Update pressure scale slider (WorldA only)
    // Note: WorldInterface doesn't expose getPressureScale(), so we can't update this
    // The pressure scale is specific to WorldA implementation

    // Update gravity button state
    // Note: WorldInterface doesn't expose getGravity(), so we can't update this

    // Update pressure strength sliders.
    if (hydrostatic_strength_slider_) {
        float strength = update.world.getHydrostaticPressureStrength();
        int slider_value = static_cast<int>(strength * 100.0f); // Convert 0.0-3.0 to 0-300
        lv_slider_set_value(hydrostatic_strength_slider_, slider_value, LV_ANIM_OFF);

        // Update label if we have it
        if (hydrostatic_strength_label_) {
            char buf[16];
            snprintf(buf, sizeof(buf), "%.1f", strength);
            lv_label_set_text(hydrostatic_strength_label_, buf);
        }
    }

    if (dynamic_strength_slider_) {
        float strength = update.world.getDynamicPressureStrength();
        int slider_value = static_cast<int>(strength * 100.0f); // Convert 0.0-3.0 to 0-300
        lv_slider_set_value(dynamic_strength_slider_, slider_value, LV_ANIM_OFF);

        // Update label if we have it
        if (dynamic_strength_label_) {
            char buf[16];
            snprintf(buf, sizeof(buf), "%.1f", strength);
            lv_label_set_text(dynamic_strength_label_, buf);
        }
    }

    // Update elasticity slider
    // Note: WorldInterface doesn't expose getElasticity(), so we can't update this
    // TODO: Fix this.

    // Update air resistance slider
    if (air_resistance_slider_) {
        float resistance = update.world.getAirResistanceStrength();
        int slider_value = static_cast<int>(resistance * 100.0f); // Convert 0.0-1.0 to 0-100
        lv_slider_set_value(air_resistance_slider_, slider_value, LV_ANIM_OFF);

        if (air_resistance_label_) {
            char buf[16];
            snprintf(buf, sizeof(buf), "%.2f", resistance);
            lv_label_set_text(air_resistance_label_, buf);
        }
    }

    // Update cohesion force slider
    if (cohesion_force_slider_) {
        float strength = update.world.getCohesionComForceStrength();
        int slider_value =
            static_cast<int>(strength * 100.0f); // Assuming 0.0-300.0 range becomes 0-30000
        lv_slider_set_value(cohesion_force_slider_, slider_value, LV_ANIM_OFF);

        if (cohesion_force_label_) {
            char buf[16];
            snprintf(buf, sizeof(buf), "%.1f", strength);
            lv_label_set_text(cohesion_force_label_, buf);
        }
    }

    spdlog::info("UI population from world complete");
}

void SimulatorUI::updateDebugButton()
{
    assert(debug_btn_ && "Debug button must be initialized");
    assert(lastWorldState_ && "World state must be available");

    lv_obj_t* label = lv_obj_get_child(debug_btn_, 0);
    if (label) {
        const char* text = lastWorldState_->isDebugDrawEnabled() ? "Debug: On" : "Debug: Off";
        lv_label_set_text(label, text);
    }
}

void SimulatorUI::updateTimescaleSlider(double timescale)
{
    if (timescale_label_) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%.2fx", timescale);
        lv_label_set_text(timescale_label_, buf);
    }

    if (timescale_slider_) {
        // Convert timescale to slider value (inverse of the logarithmic conversion)
        // timescale = pow(10.0, (value - 50) / 50.0)
        // log10(timescale) = (value - 50) / 50.0
        // value = 50 + 50 * log10(timescale)
        int32_t slider_value = static_cast<int32_t>(50 + 50 * log10(timescale));
        slider_value = std::max(0, std::min(100, slider_value)); // Clamp to valid range
        lv_slider_set_value(timescale_slider_, slider_value, LV_ANIM_OFF);
    }
}

void SimulatorUI::updateElasticitySlider(double elasticity)
{
    if (elasticity_label_) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%.2f", elasticity);
        lv_label_set_text(elasticity_label_, buf);
    }
}

void SimulatorUI::applyUpdate(const UIUpdateEvent& update)
{
    spdlog::trace("SimulatorUI::applyUpdate called with sequence {}", update.sequenceNum);

    // Store world state for rendering and comparisons.
    lastWorldState_.emplace(update.world);

    // Update FPS label.
    updateFPSLabel(update.fps);

    // Update mass label.
    updateMassLabel(update.world.getTotalMass());

    // Update pause label.
    if (pause_label_) {
        lv_label_set_text(pause_label_, update.isPaused ? "Resume" : "Pause");
    }

    // Update debug button to match world state.
    updateDebugButton();

    // Update physics sliders.
    updateTimescaleSlider(update.world.getTimescale());
    updateElasticitySlider(update.world.getElasticityFactor());

    // Update material picker selection.
    if (material_picker_) {
        material_picker_->setSelectedMaterial(update.world.getSelectedMaterial());
    }
}

void SimulatorUI::updateFPSLabel(uint32_t fps)
{
    if (fps_label_) {
        char buf[32];
        snprintf(buf, sizeof(buf), "FPS: %u", fps);
        lv_label_set_text(fps_label_, buf);
    }
}

// Get the directory containing the executable.
std::string get_executable_directory()
{
    char path[PATH_MAX];
    ssize_t count = readlink("/proc/self/exe", path, PATH_MAX);
    if (count == -1) {
        printf("Failed to get executable path\n");
        return ".";
    }
    path[count] = '\0';
    char* dir = dirname(path);
    return std::string(dir);
}

// PNG writer using LODEPNG.
void write_png_file(const char* filename, const uint8_t* rgb_data, uint32_t width, uint32_t height)
{
    std::vector<uint8_t> corrected_data(width * height * 3);
    for (uint32_t i = 0; i < width * height; i++) {
        uint32_t src_idx = i * 3;
        uint32_t dst_idx = i * 3;
        corrected_data[dst_idx + 0] = rgb_data[src_idx + 2];
        corrected_data[dst_idx + 1] = rgb_data[src_idx + 1];
        corrected_data[dst_idx + 2] = rgb_data[src_idx + 0];
    }

    unsigned char* png_data;
    size_t png_size;

    unsigned error = lodepng_encode24(&png_data, &png_size, corrected_data.data(), width, height);

    if (error) {
        printf("PNG encoding error %u: %s\n", error, lodepng_error_text(error));
        return;
    }

    std::ofstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        printf("Failed to open file: %s\n", filename);
        free(png_data);
        return;
    }

    file.write(reinterpret_cast<const char*>(png_data), png_size);
    file.close();
    free(png_data);

    printf("Screenshot saved: %s (%zu bytes)\n", filename, png_size);
}

void SimulatorUI::takeExitScreenshot()
{
    lv_obj_t* screen = lv_scr_act();
    if (!screen) {
        printf("No active screen found for exit screenshot\n");
        return;
    }

    lv_draw_buf_t* snapshot = lv_snapshot_take(screen, LV_COLOR_FORMAT_RGB888);
    if (!snapshot) {
        printf("Failed to take exit screenshot\n");
        return;
    }

    const std::string exec_dir = get_executable_directory();
    const std::string filename = exec_dir + "/screenshot-last-exit.png";

    const uint8_t* rgb_data = static_cast<const uint8_t*>(snapshot->data);
    const uint32_t width = snapshot->header.w;
    const uint32_t height = snapshot->header.h;

    write_png_file(filename.c_str(), rgb_data, width, height);

    lv_draw_buf_destroy(snapshot);

    printf("Exit screenshot saved as: %s\n", filename.c_str());
}
