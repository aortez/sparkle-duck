#include "SimulatorUI.h"
#include "Cell.h"
#include "Event.h"
#include "EventRouter.h"
#include "MaterialType.h"
#include "SharedSimState.h"
#include "SimulationManager.h"
#include "SparkleAssert.h"
#include "UIUpdateConsumer.h"
#include "WorldB.h"
#include "WorldFactory.h"
#include "WorldInterface.h"
#include "WorldState.h"
#include "lvgl/lvgl.h"
#include "lvgl/src/misc/lv_timer.h"
#include "lvgl/src/others/snapshot/lv_snapshot.h"
#include "scenarios/Scenario.h"
#include "scenarios/ScenarioRegistry.h"
#include "scenarios/ScenarioWorldSetup.h"
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
    : world_(nullptr),
      manager_(nullptr),
      event_router_(eventRouter),
      screen_(screen),
      draw_area_(nullptr),
      mass_label_(nullptr),
      fps_label_(nullptr),
      pause_label_(nullptr),
      world_type_btnm_(nullptr),
      timescale_(1.0),
      is_paused_(false),
      frame_limiting_enabled_(true), // Default to frame limiting enabled.
      interaction_mode_(InteractionMode::NONE),
      paint_material_(MaterialType::DIRT)
{
    // Pre-reserve capacity for callback data to prevent reallocation
    callback_data_storage_.reserve(200);
}

void SimulatorUI::setWorld(WorldInterface* world)
{
    world_ = world;
    // Update all existing callback data.
    for (auto& data : callback_data_storage_) {
        data->world = world_;
    }
}

void SimulatorUI::setSimulationManager(SimulationManager* manager)
{
    manager_ = manager;
    // Update all existing callback data.
    for (auto& data : callback_data_storage_) {
        data->manager = manager_;
    }
}

SimulatorUI::CallbackData* SimulatorUI::createCallbackData(lv_obj_t* label)
{
    auto data = std::make_unique<CallbackData>();
    data->ui = this;
    data->world = world_;
    data->manager = manager_;
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

    createDrawArea();
    createLabels();
    createWorldTypeColumn();
    createMaterialPicker();
    createControlButtons();
    createSliders();

    // Set initial button matrix state based on current world type.
    if (world_) {
        updateWorldTypeButtonMatrix(world_->getWorldType());
    }

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
    lv_obj_align(mass_label_, LV_ALIGN_TOP_LEFT, MAIN_CONTROLS_X, 10);

    // Create FPS label - positioned over the world area.
    fps_label_ = lv_label_create(screen_);
    lv_label_set_text(fps_label_, "FPS: 0");
    lv_obj_align(fps_label_, LV_ALIGN_TOP_LEFT, 10, 10); // Top-left corner of world area.

    // Create frame limiting toggle button below FPS display.
    LVGLEventBuilder::button(screen_, event_router_)
        .onFrameLimitToggle()
        .size(120, 30)
        .position(10, 40, LV_ALIGN_TOP_LEFT)
        .text("Limit: On")
        .buildOrLog();
}

void SimulatorUI::createWorldTypeColumn()
{
    // Create world type label.
    lv_obj_t* world_type_label = lv_label_create(screen_);
    lv_label_set_text(world_type_label, "World Type:");
    lv_obj_align(world_type_label, LV_ALIGN_TOP_LEFT, WORLD_TYPE_COLUMN_X, 10);

    // Create world type button matrix with vertical stack.
    static const char* world_btnm_map[] = { "WorldA", "\n", "WorldB", "" };

    world_type_btnm_ =
        LVGLEventBuilder::buttonMatrix(screen_, event_router_)
            .map(world_btnm_map)
            .size(WORLD_TYPE_COLUMN_WIDTH, 100)
            .position(WORLD_TYPE_COLUMN_X, 30, LV_ALIGN_TOP_LEFT)
            .oneChecked(true)
            .buttonCtrl(0, LV_BUTTONMATRIX_CTRL_CHECKABLE)
            .buttonCtrl(1, LV_BUTTONMATRIX_CTRL_CHECKABLE)
            .selectedButton(1) // WorldB is default.
            .style(
                LV_PART_ITEMS,
                [](lv_style_t* style) {
                    lv_style_set_bg_color(style, lv_color_hex(0x404040));
                    lv_style_set_text_color(style, lv_color_white());
                })
            .style(
                static_cast<lv_style_selector_t>(
                    static_cast<int>(LV_PART_ITEMS) | static_cast<int>(LV_STATE_CHECKED)),
                [](lv_style_t* style) { lv_style_set_bg_color(style, lv_color_hex(0x0080FF)); })
            .onWorldTypeSelect()
            .buildOrLog();

    // Create scenario controls after world type buttons.
    // Scenario label.
    lv_obj_t* scenario_label = lv_label_create(screen_);
    lv_label_set_text(scenario_label, "Scenario:");
    lv_obj_align(scenario_label, LV_ALIGN_TOP_LEFT, WORLD_TYPE_COLUMN_X, 135);

    // Scenario dropdown.
    scenario_dropdown_ = lv_dropdown_create(screen_);
    lv_obj_set_size(scenario_dropdown_, WORLD_TYPE_COLUMN_WIDTH, 30);
    lv_obj_align(scenario_dropdown_, LV_ALIGN_TOP_LEFT, WORLD_TYPE_COLUMN_X, 155);

    // Populate dropdown with scenarios from registry.
    updateScenarioDropdown();

    // Set event callback for scenario selection.
    lv_obj_add_event_cb(
        scenario_dropdown_, onScenarioChanged, LV_EVENT_VALUE_CHANGED, createCallbackData());
}

void SimulatorUI::createMaterialPicker()
{
    // Create material picker label.
    lv_obj_t* material_label = lv_label_create(screen_);
    lv_label_set_text(material_label, "Materials:");
    lv_obj_align(
        material_label, LV_ALIGN_TOP_LEFT, WORLD_TYPE_COLUMN_X, 195); // Below scenario dropdown.

    // Create material picker container.
    lv_obj_t* picker_container = lv_obj_create(screen_);
    lv_obj_set_size(
        picker_container, WORLD_TYPE_COLUMN_WIDTH, 320); // Give enough space for 4x2 grid.
    lv_obj_align(picker_container, LV_ALIGN_TOP_LEFT, WORLD_TYPE_COLUMN_X, 215); // Below label.
    lv_obj_set_style_pad_all(picker_container, 5, 0);
    lv_obj_set_style_border_width(picker_container, 1, 0);
    lv_obj_set_style_border_color(picker_container, lv_color_hex(0x606060), 0);

    // Create MaterialPicker instance with event router.
    material_picker_ = std::make_unique<MaterialPicker>(picker_container, event_router_);
    material_picker_->createMaterialSelector();

    spdlog::info("Material picker created in SimulatorUI");
}

void SimulatorUI::onMaterialSelectionChanged(MaterialType newMaterial)
{
    spdlog::info("Material selection changed to: {}", getMaterialName(newMaterial));

    // Update the world's selected material.
    if (world_) {
        world_->setSelectedMaterial(newMaterial);
    }

    // Update paint material for paint mode.
    paint_material_ = newMaterial;
}

void SimulatorUI::createControlButtons()
{
    // Create debug toggle button.
    if (event_router_) {
        debug_btn_ = LVGLEventBuilder::button(screen_, event_router_)
                         .onDebugToggle()
                         .size(CONTROL_WIDTH, 50)
                         .position(MAIN_CONTROLS_X, 10)
                         .text("Debug: Off")
                         .buildOrLog();
    }

    // === WorldA Pressure Controls ===.
    lv_obj_t* worldA_pressure_header = lv_label_create(screen_);
    lv_label_set_text(worldA_pressure_header, "=== WorldA Pressure ===");
    lv_obj_align(worldA_pressure_header, LV_ALIGN_TOP_LEFT, MAIN_CONTROLS_X, 70);

    // Create pressure system dropdown.
    lv_obj_t* pressure_label = lv_label_create(screen_);
    lv_label_set_text(pressure_label, "System:");
    lv_obj_align(pressure_label, LV_ALIGN_TOP_LEFT, MAIN_CONTROLS_X, 95);

    LVGLEventBuilder::dropdown(screen_, event_router_)
        .onPressureSystemChange()
        .size(CONTROL_WIDTH, 40)
        .position(MAIN_CONTROLS_X, 115, LV_ALIGN_TOP_LEFT)
        .options("Original (COM)\nTop-Down Hydrostatic\nIterative Settling")
        .selected(0)
        .buildOrLog();

    // Pressure scale slider (WorldA only).
    LVGLEventBuilder::slider(screen_, event_router_)
        .onPressureScaleChange()
        .position(MAIN_CONTROLS_X, 185, LV_ALIGN_TOP_LEFT)
        .size(CONTROL_WIDTH, 10)
        .range(0, 1000)
        .value(100)
        .label("Strength", 0, -20)
        .valueLabel("%.1f", 135, -20)
        .buildOrLog();

    // Create gravity slider (-10x to +10x Earth gravity).
    LVGLEventBuilder::slider(screen_, event_router_)
        .onGravityChange()
        .position(MAIN_CONTROLS_X, 245, LV_ALIGN_TOP_LEFT)
        .size(CONTROL_WIDTH, 10)
        .range(-1000, 1000) // -10x to +10x.
        .value(100)         // 1x Earth gravity (9.81).
        .label("Gravity", 0, -20)
        .valueLabel("%.1f", 80, -20)
        .buildOrLog();

    // Create viscosity strength slider.
    LVGLEventBuilder::slider(screen_, event_router_)
        .onViscosityStrengthChange()
        .position(MAIN_CONTROLS_X, 285, LV_ALIGN_TOP_LEFT)
        .size(CONTROL_WIDTH, 10)
        .range(0, 200)
        .value(100)
        .label("Viscosity", 0, -20)
        .valueLabel("%.1f", 80, -20)
        .buildOrLog();

    // Create cohesion force toggle slider (integrated switch + slider).
    cohesion_switch_ = LVGLEventBuilder::toggleSlider(screen_, event_router_)
                           .label("Cohesion Force")
                           .position(MAIN_CONTROLS_X, 320, LV_ALIGN_TOP_LEFT)
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
        .position(MAIN_CONTROLS_X, 405, LV_ALIGN_TOP_LEFT)
        .size(CONTROL_WIDTH, 10)
        .range(1, 5)
        .value(1)
        .label("Cohesion Range", 0, -20)
        .valueLabel("%.0f", 120, -20)
        .buildOrLog();

    // Create friction toggle slider (integrated switch + slider).
    LVGLEventBuilder::toggleSlider(screen_, event_router_)
        .label("Friction")
        .position(MAIN_CONTROLS_X, 450, LV_ALIGN_TOP_LEFT)
        .sliderWidth(CONTROL_WIDTH)
        .range(0, 100)
        .value(100)
        .defaultValue(100)
        .valueScale(0.01)
        .valueFormat("%.2f")
        .initiallyEnabled(true)
        .onValueChange([](double value) {
            return Event{SetFrictionStrengthCommand{value}};
        })
        .buildOrLog();

    // Create adhesion toggle slider (integrated switch + slider).
    adhesion_switch_ = LVGLEventBuilder::toggleSlider(screen_, event_router_)
                           .label("Adhesion")
                           .position(MAIN_CONTROLS_X, 530, LV_ALIGN_TOP_LEFT)
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

    // Create left throw toggle button.
    LVGLEventBuilder::button(screen_, event_router_)
        .onLeftThrowToggle()
        .size(CONTROL_WIDTH, 50)
        .position(MAIN_CONTROLS_X, 610, LV_ALIGN_TOP_LEFT)
        .text("Left Throw: On")
        .buildOrLog();

    // Create right throw toggle button.
    LVGLEventBuilder::button(screen_, event_router_)
        .onRightThrowToggle()
        .size(CONTROL_WIDTH, 50)
        .position(MAIN_CONTROLS_X, 670, LV_ALIGN_TOP_LEFT)
        .text("Right Throw: On")
        .buildOrLog();

    // Create quadrant toggle button.
    LVGLEventBuilder::button(screen_, event_router_)
        .onQuadrantToggle()
        .size(CONTROL_WIDTH, 50)
        .position(MAIN_CONTROLS_X, 730, LV_ALIGN_TOP_LEFT)
        .text("Quadrant: On")
        .buildOrLog();

    // Create screenshot button.
    LVGLEventBuilder::button(screen_, event_router_)
        .onScreenshot() // Call event method first.
        .size(CONTROL_WIDTH, 50)
        .position(MAIN_CONTROLS_X, 790, LV_ALIGN_TOP_LEFT)
        .text("Screenshot")
        .buildOrLog();

    // Create print ASCII button.
    LVGLEventBuilder::button(screen_, event_router_)
        .onPrintAscii()
        .size(CONTROL_WIDTH, 50)
        .position(MAIN_CONTROLS_X, 850, LV_ALIGN_TOP_LEFT)
        .text("Print ASCII")
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
    const int SLIDER_COLUMN_X = MAIN_CONTROLS_X + CONTROL_WIDTH + 10;

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
    spdlog::info("Creating cell size slider - Cell::getSize() returns: {}", Cell::getSize());
    LVGLEventBuilder::slider(screen_, event_router_)
        .onCellSizeChange()
        .position(SLIDER_COLUMN_X, 350, LV_ALIGN_TOP_LEFT)
        .size(CONTROL_WIDTH, 10)
        .range(10, 100)
        .value(Cell::getSize())
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

    // === WorldB Pressure Controls ===.
    lv_obj_t* worldB_pressure_header = lv_label_create(screen_);
    lv_label_set_text(worldB_pressure_header, "=== WorldB Pressure ===");
    lv_obj_align(worldB_pressure_header, LV_ALIGN_TOP_LEFT, SLIDER_COLUMN_X, 620);

    // Hydrostatic pressure toggle.
    lv_obj_t* hydrostatic_label = lv_label_create(screen_);
    lv_label_set_text(hydrostatic_label, "Hydrostatic Pressure");
    lv_obj_align(hydrostatic_label, LV_ALIGN_TOP_LEFT, SLIDER_COLUMN_X, 645);

    hydrostatic_switch_ = LVGLEventBuilder::lvSwitch(screen_, event_router_)
                              .onHydrostaticPressureToggle()
                              .position(SLIDER_COLUMN_X + 180, 645, LV_ALIGN_TOP_LEFT)
                              .checked(false)
                              .buildOrLog();

    // Dynamic pressure toggle.
    lv_obj_t* dynamic_label = lv_label_create(screen_);
    lv_label_set_text(dynamic_label, "Dynamic Pressure");
    lv_obj_align(dynamic_label, LV_ALIGN_TOP_LEFT, SLIDER_COLUMN_X, 675);

    dynamic_switch_ = LVGLEventBuilder::lvSwitch(screen_, event_router_)
                          .onDynamicPressureToggle()
                          .position(SLIDER_COLUMN_X + 180, 675, LV_ALIGN_TOP_LEFT)
                          .checked(false)
                          .buildOrLog();

    // Pressure diffusion toggle.
    lv_obj_t* diffusion_label = lv_label_create(screen_);
    lv_label_set_text(diffusion_label, "Pressure Diffusion");
    lv_obj_align(diffusion_label, LV_ALIGN_TOP_LEFT, SLIDER_COLUMN_X, 705);

    diffusion_switch_ = LVGLEventBuilder::lvSwitch(screen_, event_router_)
                            .onPressureDiffusionToggle()
                            .position(SLIDER_COLUMN_X + 180, 705, LV_ALIGN_TOP_LEFT)
                            .checked(false)
                            .buildOrLog();

    // Hydrostatic pressure strength slider (WorldB only).
    LVGLEventBuilder::slider(screen_, event_router_)
        .onHydrostaticPressureStrengthChange()
        .position(SLIDER_COLUMN_X, 765, LV_ALIGN_TOP_LEFT)
        .size(CONTROL_WIDTH, 10)
        .range(0, 300)
        .value(100)
        .label("Hydrostatic Strength", 0, -20)
        .valueLabel("%.1f", 140, -20)
        .buildOrLog();

    // Dynamic pressure strength slider (WorldB only) - migrated to EventRouter.
    LVGLEventBuilder::slider(screen_, event_router_)
        .onDynamicStrengthChange() // Uses new event system.
        .position(SLIDER_COLUMN_X, 815, LV_ALIGN_TOP_LEFT)
        .size(CONTROL_WIDTH, 10)
        .range(0, 300) // 0.0 to 3.0 range.
        .value(100)    // Default 1.0 -> 100.
        .label("Dynamic Strength", 0, -20)
        .valueLabel("%.1f", 140, -20)
        .buildOrLog();

    // Air resistance slider.
    LVGLEventBuilder::slider(screen_, event_router_)
        .onAirResistanceChange()
        .position(SLIDER_COLUMN_X, 865, LV_ALIGN_TOP_LEFT)
        .size(CONTROL_WIDTH, 10)
        .range(0, 100)
        .value(10)
        .label("Air Resistance", 0, -20)
        .valueLabel("%.2f", 120, -20)
        .buildOrLog();

    // Pressure scale slider for WorldB.
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

void SimulatorUI::populateFromWorld()
{
    if (!world_) {
        spdlog::warn("populateFromWorld called without world_ set");
        return;
    }

    spdlog::info("Populating UI controls from world values");

    // Update labels
    updateMassLabel(world_->getTotalMass());

    // Update world type button matrix
    updateWorldTypeButtonMatrix(world_->getWorldType());

    // Update material selection
    if (material_picker_) {
        material_picker_->setSelectedMaterial(world_->getSelectedMaterial());
    }

    // Update pressure control switches (merged from updatePressureControlsFromWorld)
    if (hydrostatic_switch_) {
        bool enabled = world_->isHydrostaticPressureEnabled();
        if (enabled) {
            lv_obj_add_state(hydrostatic_switch_, LV_STATE_CHECKED);
        }
        else {
            lv_obj_clear_state(hydrostatic_switch_, LV_STATE_CHECKED);
        }
    }

    if (dynamic_switch_) {
        bool enabled = world_->isDynamicPressureEnabled();
        if (enabled) {
            lv_obj_add_state(dynamic_switch_, LV_STATE_CHECKED);
        }
        else {
            lv_obj_clear_state(dynamic_switch_, LV_STATE_CHECKED);
        }
    }

    if (diffusion_switch_) {
        bool enabled = world_->isPressureDiffusionEnabled();
        if (enabled) {
            lv_obj_add_state(diffusion_switch_, LV_STATE_CHECKED);
        }
        else {
            lv_obj_clear_state(diffusion_switch_, LV_STATE_CHECKED);
        }
    }

    // Update sliders based on world properties
    WorldType worldType = world_->getWorldType();

    // Update pressure scale slider (WorldA only)
    // Note: WorldInterface doesn't expose getPressureScale(), so we can't update this
    // The pressure scale is specific to WorldA implementation

    // Update gravity button state
    // Note: WorldInterface doesn't expose getGravity(), so we can't update this

    // Update pressure strength sliders (WorldB only)
    if (worldType == WorldType::RulesB) {
        if (hydrostatic_strength_slider_) {
            float strength = world_->getHydrostaticPressureStrength();
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
            float strength = world_->getDynamicPressureStrength();
            int slider_value = static_cast<int>(strength * 100.0f); // Convert 0.0-3.0 to 0-300
            lv_slider_set_value(dynamic_strength_slider_, slider_value, LV_ANIM_OFF);

            // Update label if we have it
            if (dynamic_strength_label_) {
                char buf[16];
                snprintf(buf, sizeof(buf), "%.1f", strength);
                lv_label_set_text(dynamic_strength_label_, buf);
            }
        }
    }

    // Update elasticity slider
    // Note: WorldInterface doesn't expose getElasticity(), so we can't update this
    // TODO: Fix this.

    // Update air resistance slider
    if (air_resistance_slider_) {
        float resistance = world_->getAirResistanceStrength();
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
        float strength = world_->getCohesionComForceStrength();
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
    if (debug_btn_ && world_) {
        lv_obj_t* label = lv_obj_get_child(debug_btn_, 0);
        if (label) {
            lv_label_set_text(label, world_->isDebugDrawEnabled() ? "Debug: On" : "Debug: Off");
        }
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
    // Use dirty flags to update only what has changed for efficiency.

    // Update FPS label if FPS changed.
    if (update.dirty.fps) {
        updateFPSLabel(update.fps);
    }

    // Update simulation stats if they changed.
    if (update.dirty.stats) {
        // Update total mass label.
        updateMassLabel(update.stats.totalMass);

        // Note: Other stats like cell counts, pressure, etc. could be displayed.
        // if we had UI elements for them. For now we just update mass.
    }

    // Update UI state elements if they changed.
    if (update.dirty.uiState) {
        // Update pause label to show the current state.
        // Note: We don't manually sync the button's checked state because the button
        // manages its own toggle state when clicked. Trying to manually sync it
        // can cause conflicts and race conditions.
        if (pause_label_) {
            lv_label_set_text(pause_label_, update.isPaused ? "Resume" : "Pause");
        }

        // Update debug button.
        if (world_ && update.debugEnabled != world_->isDebugDrawEnabled()) {
            world_->setDebugDrawEnabled(update.debugEnabled);
            updateDebugButton();
        }

        // Update other toggle states.
        // Note: These affect the world state directly, so we need to be careful.
        // about thread safety. For now, we'll just track the state.
        // TODO: Consider if these should be applied to the world or just displayed.

        // Force visualization - this would need a force button/label to update.
        // update.forceEnabled.

        // Cohesion enabled - this would need a cohesion button/label to update.
        // update.cohesionEnabled.

        // Adhesion enabled - this would need an adhesion button/label to update.
        // update.adhesionEnabled.

        // Time history enabled - this would need a time history button/label to update.
        // update.timeHistoryEnabled.
    }

    // Update physics parameters if they changed.
    if (update.dirty.physicsParams) {
        // Update slider value labels to reflect current physics parameters.
        updateTimescaleSlider(update.physicsParams.timescale);
        updateElasticitySlider(update.physicsParams.elasticity);

        // Note: We could also update the actual slider positions here if needed,
        // but for now we just update the labels to show the current values.
    }

    // Update world state if it changed.
    if (update.dirty.worldState) {
        // Update world type button matrix.
        if (update.worldType == "WorldA") {
            updateWorldTypeButtonMatrix(WorldType::RulesA);
        }
        else if (update.worldType == "WorldB") {
            updateWorldTypeButtonMatrix(WorldType::RulesB);
        }

        // Update material picker if selected material changed.
        if (material_picker_ && world_) {
            // Check if the selected material actually changed.
            MaterialType currentMaterial = world_->getSelectedMaterial();
            if (currentMaterial != update.selectedMaterial) {
                // This might need synchronization or deferred update.
                // For now, just log the discrepancy.
                spdlog::trace(
                    "Selected material mismatch: UI has {}, update has {}",
                    static_cast<int>(currentMaterial),
                    static_cast<int>(update.selectedMaterial));
            }
        }
    }
}
// Static event callback implementations.
void SimulatorUI::drawAreaEventCb(lv_event_t* e)
{
    lv_event_code_t code = lv_event_get_code(e);
    CallbackData* data = static_cast<CallbackData*>(lv_event_get_user_data(e));
    spdlog::info(
        "Draw area event: code={}, data={}, world={}",
        (int)code,
        (void*)data,
        data ? (void*)data->world : nullptr);

    if (!data || !data->world) {
        spdlog::error("Draw area event but data or world is null!");
        return;
    }
    WorldInterface* world_ptr = data->world;

    lv_point_t point;
    lv_indev_get_point(lv_indev_get_act(), &point);

    lv_area_t area;
    lv_obj_get_coords(static_cast<lv_obj_t*>(lv_event_get_target(e)), &area);

    point.x -= area.x1;
    point.y -= area.y1;

    if (code == LV_EVENT_PRESSED) {
        // Mode-based interaction: grab existing material or start painting.
        bool hasMaterial = world_ptr->hasMaterialAtPixel(point.x, point.y);
        MaterialType selectedMaterial = world_ptr->getSelectedMaterial();

        if (hasMaterial) {
            // Cell has material - enter GRAB_MODE.
            data->ui->interaction_mode_ = data->ui->InteractionMode::GRAB_MODE;
            spdlog::info(
                "Mouse pressed at ({},{}) - GRAB_MODE: starting drag of existing material",
                point.x,
                point.y);
            world_ptr->startDragging(point.x, point.y);
        }
        else {
            // Cell is empty - enter PAINT_MODE.
            data->ui->interaction_mode_ = data->ui->InteractionMode::PAINT_MODE;
            data->ui->paint_material_ = selectedMaterial;
            spdlog::info(
                "Mouse pressed at ({},{}) - PAINT_MODE: painting {} material",
                point.x,
                point.y,
                getMaterialName(selectedMaterial));
            world_ptr->addMaterialAtPixel(point.x, point.y, selectedMaterial);
        }
    }
    else if (code == LV_EVENT_PRESSING) {
        // Handle both grab and paint modes during drag.
        if (data->ui->interaction_mode_ == data->ui->InteractionMode::GRAB_MODE) {
            spdlog::info("Mouse pressing at ({},{}) - GRAB_MODE: updating drag", point.x, point.y);
            world_ptr->updateDrag(point.x, point.y);
        }
        else if (data->ui->interaction_mode_ == data->ui->InteractionMode::PAINT_MODE) {
            spdlog::info(
                "Mouse pressing at ({},{}) - PAINT_MODE: painting {} material",
                point.x,
                point.y,
                getMaterialName(data->ui->paint_material_));
            world_ptr->addMaterialAtPixel(point.x, point.y, data->ui->paint_material_);
        }
    }
    else if (code == LV_EVENT_RELEASED) {
        // Handle both grab and paint modes on release.
        if (data->ui->interaction_mode_ == data->ui->InteractionMode::GRAB_MODE) {
            spdlog::info("Mouse released at ({},{}) - GRAB_MODE: ending drag", point.x, point.y);
            world_ptr->endDragging(point.x, point.y);
        }
        else if (data->ui->interaction_mode_ == data->ui->InteractionMode::PAINT_MODE) {
            spdlog::info(
                "Mouse released at ({},{}) - PAINT_MODE: finished painting", point.x, point.y);
            // No special action needed for paint mode - just stop painting.
        }

        // Reset interaction mode.
        data->ui->interaction_mode_ = data->ui->InteractionMode::NONE;

        // Mark all cells dirty to ensure proper rendering updates.
        world_ptr->markAllCellsDirty();
    }
}

void SimulatorUI::pauseBtnEventCb(lv_event_t* e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        CallbackData* data = static_cast<CallbackData*>(lv_event_get_user_data(e));
        if (data && data->ui) {
            data->ui->is_paused_ = !data->ui->is_paused_;
            lv_label_set_text(data->ui->pause_label_, data->ui->is_paused_ ? "Resume" : "Pause");
            if (data->world) {
                data->world->setTimescale(data->ui->is_paused_ ? 0.0 : data->ui->timescale_);
            }
        }
    }
}

void SimulatorUI::timescaleSliderEventCb(lv_event_t* e)
{
    lv_obj_t* slider = static_cast<lv_obj_t*>(lv_event_get_target(e));
    CallbackData* data = static_cast<CallbackData*>(lv_event_get_user_data(e));
    if (lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED && data) {
        int32_t value = lv_slider_get_value(slider);
        double timescale = pow(10.0, (value - 50) / 50.0);
        data->ui->timescale_ = timescale;
        if (data->world && !data->ui->is_paused_) {
            data->world->setTimescale(timescale);
        }
        char buf[16];
        snprintf(buf, sizeof(buf), "%.2fx", timescale);
        lv_label_set_text(data->associated_label, buf);
    }
}

void SimulatorUI::resetBtnEventCb(lv_event_t* e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        CallbackData* data = static_cast<CallbackData*>(lv_event_get_user_data(e));
        spdlog::info(
            "Reset button clicked: data={}, manager={}",
            (void*)data,
            data ? (void*)data->manager : nullptr);
        if (data && data->manager) {
            spdlog::info("Calling reset on simulation manager {}", (void*)data->manager);
            data->manager->reset();
            spdlog::info("Reset completed");

            // Update UI controls to reflect the new world state after reset.
            // This is important because scenarios can change physics parameters.
            if (data->ui) {
                data->ui->populateFromWorld();
            }
        }
        else {
            spdlog::error("Reset button clicked but data or manager is null!");
        }
    }
}

void SimulatorUI::debugBtnEventCb(lv_event_t* e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        CallbackData* data = static_cast<CallbackData*>(lv_event_get_user_data(e));
        if (data && data->world) {
            bool current = data->world->isDebugDrawEnabled();
            data->world->setDebugDrawEnabled(!current);
            const lv_obj_t* btn = static_cast<const lv_obj_t*>(lv_event_get_target(e));
            lv_obj_t* label = lv_obj_get_child(btn, 0);
            lv_label_set_text(
                label, data->world->isDebugDrawEnabled() ? "Debug: On" : "Debug: Off");

            // Mark all cells dirty to ensure proper rendering updates.
            data->world->markAllCellsDirty();
        }
    }
}

void SimulatorUI::pressureSystemDropdownEventCb(lv_event_t* e)
{
    if (lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED) {
        CallbackData* data = static_cast<CallbackData*>(lv_event_get_user_data(e));
        if (data && data->world) {
            // Only apply pressure system changes to WorldA (RulesA).
            if (data->world->getWorldType() != WorldType::RulesA) {
                spdlog::info("Pressure system dropdown only affects WorldA (RulesA) - current "
                             "world is WorldB (RulesB)");
                return;
            }

            lv_obj_t* dropdown = static_cast<lv_obj_t*>(lv_event_get_target(e));
            uint16_t selected = lv_dropdown_get_selected(dropdown);

            // Map dropdown selection to PressureSystem enum.
            WorldInterface::PressureSystem system;
            switch (selected) {
                case 0:
                    system = WorldInterface::PressureSystem::Original;
                    break;
                case 1:
                    system = WorldInterface::PressureSystem::TopDown;
                    break;
                case 2:
                    system = WorldInterface::PressureSystem::IterativeSettling;
                    break;
                default:
                    system = WorldInterface::PressureSystem::Original;
                    break;
            }

            // Update the world's pressure system.
            data->world->setPressureSystem(system);

            // Optional: Print confirmation to console.
            const char* system_names[] = { "Original (COM)",
                                           "Top-Down Hydrostatic",
                                           "Iterative Settling" };
            spdlog::info("Pressure system switched to: {}", system_names[selected]);
        }
    }
}

void SimulatorUI::gravityBtnEventCb(lv_event_t* e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        CallbackData* data = static_cast<CallbackData*>(lv_event_get_user_data(e));
        if (data && data->world) {
            static bool gravity_enabled = true;
            gravity_enabled = !gravity_enabled;
            data->world->setGravity(gravity_enabled ? 9.81 : 0.0);
            const lv_obj_t* btn = static_cast<const lv_obj_t*>(lv_event_get_target(e));
            lv_obj_t* label = lv_obj_get_child(btn, 0);
            lv_label_set_text(label, gravity_enabled ? "Gravity: On" : "Gravity: Off");
        }
    }
}

void SimulatorUI::cohesionBtnEventCb(lv_event_t* e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        CallbackData* data = static_cast<CallbackData*>(lv_event_get_user_data(e));
        if (data && data->world) {
            bool current_state = data->world->isCohesionBindForceEnabled();
            bool new_state = !current_state;
            data->world->setCohesionBindForceEnabled(new_state);
            const lv_obj_t* btn = static_cast<const lv_obj_t*>(lv_event_get_target(e));
            lv_obj_t* label = lv_obj_get_child(btn, 0);
            lv_label_set_text(label, new_state ? "Viscosity: On" : "Viscosity: Off");
        }
    }
}

void SimulatorUI::cohesionForceBtnEventCb(lv_event_t* e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        CallbackData* data = static_cast<CallbackData*>(lv_event_get_user_data(e));
        if (data && data->world) {
            bool current_state = data->world->isCohesionComForceEnabled();
            bool new_state = !current_state;
            data->world->setCohesionComForceEnabled(new_state);
            const lv_obj_t* btn = static_cast<const lv_obj_t*>(lv_event_get_target(e));
            lv_obj_t* label = lv_obj_get_child(btn, 0);
            lv_label_set_text(label, new_state ? "Cohesion Force: On" : "Cohesion Force: Off");
        }
    }
}

void SimulatorUI::adhesionBtnEventCb(lv_event_t* e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        CallbackData* data = static_cast<CallbackData*>(lv_event_get_user_data(e));
        if (data && data->world) {
            bool current_state = data->world->isAdhesionEnabled();
            bool new_state = !current_state;
            data->world->setAdhesionEnabled(new_state);
            Cell::adhesionDrawEnabled = new_state; // Also control vector display.
            const lv_obj_t* btn = static_cast<const lv_obj_t*>(lv_event_get_target(e));
            lv_obj_t* label = lv_obj_get_child(btn, 0);
            lv_label_set_text(label, new_state ? "Adhesion: On" : "Adhesion: Off");

            // Mark all cells dirty to ensure proper rendering updates.
            data->world->markAllCellsDirty();
        }
    }
}

void SimulatorUI::frameLimitBtnEventCb(lv_event_t* e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        CallbackData* data = static_cast<CallbackData*>(lv_event_get_user_data(e));
        if (data && data->ui) {
            data->ui->frame_limiting_enabled_ = !data->ui->frame_limiting_enabled_;
            const lv_obj_t* btn = static_cast<const lv_obj_t*>(lv_event_get_target(e));
            lv_obj_t* label = lv_obj_get_child(btn, 0);
            lv_label_set_text(
                label, data->ui->frame_limiting_enabled_ ? "Limit: On" : "Limit: Off");
        }
    }
}

void SimulatorUI::elasticitySliderEventCb(lv_event_t* e)
{
    lv_obj_t* slider = static_cast<lv_obj_t*>(lv_event_get_target(e));
    CallbackData* data = static_cast<CallbackData*>(lv_event_get_user_data(e));
    if (lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED && data) {
        int32_t value = lv_slider_get_value(slider);
        double elasticity = value / 100.0;
        if (data->world) {
            data->world->setElasticityFactor(elasticity);
        }
        char buf[16];
        snprintf(buf, sizeof(buf), "%.2f", elasticity);
        lv_label_set_text(data->associated_label, buf);
    }
}

void SimulatorUI::fragmentationSliderEventCb(lv_event_t* e)
{
    lv_obj_t* slider = static_cast<lv_obj_t*>(lv_event_get_target(e));
    CallbackData* data = static_cast<CallbackData*>(lv_event_get_user_data(e));
    if (lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED && data) {
        int32_t value = lv_slider_get_value(slider);
        double fragmentation_factor = value / 100.0;
        if (data->world) {
            data->world->setDirtFragmentationFactor(fragmentation_factor);
        }
        char buf[16];
        snprintf(buf, sizeof(buf), "%.2f", fragmentation_factor);
        lv_label_set_text(data->associated_label, buf);
    }
}

void SimulatorUI::pressureScaleSliderEventCb(lv_event_t* e)
{
    lv_obj_t* slider = static_cast<lv_obj_t*>(lv_event_get_target(e));
    CallbackData* data = static_cast<CallbackData*>(lv_event_get_user_data(e));
    if (lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED && data) {
        int32_t value = lv_slider_get_value(slider);
        double pressure_scale = value / 100.0;
        if (data->world) {
            // Only apply pressure scale changes to WorldA (RulesA).
            if (data->world->getWorldType() != WorldType::RulesA) {
                spdlog::debug("Pressure scale slider only affects WorldA (RulesA) - current world "
                              "is WorldB (RulesB)");
                // Still update the label to show the value for consistency.
                char buf[16];
                snprintf(buf, sizeof(buf), "%.1f", pressure_scale);
                lv_label_set_text(data->associated_label, buf);
                return;
            }
            data->world->setPressureScale(pressure_scale);
        }
        char buf[16];
        snprintf(buf, sizeof(buf), "%.1f", pressure_scale);
        lv_label_set_text(data->associated_label, buf);
    }
}

void SimulatorUI::cellSizeSliderEventCb(lv_event_t* e)
{
    lv_obj_t* slider = static_cast<lv_obj_t*>(lv_event_get_target(e));
    CallbackData* data = static_cast<CallbackData*>(lv_event_get_user_data(e));
    if (lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED && data) {
        int32_t value = lv_slider_get_value(slider);
        spdlog::info("Setting cell size to {}", value);
        Cell::setSize(value);

        // Recalculate grid dimensions based on new cell size.
        // (One fewer than would fit perfectly, same logic as main.cpp).
        const int new_grid_width = (DRAW_AREA_SIZE / value) - 1;
        const int new_grid_height = (DRAW_AREA_SIZE / value) - 1;

        // Resize the world grid if we have a valid world.
        // For cell size changes, preserve time reversal history.
        if (data->world) {
            data->world->resizeGrid(new_grid_width, new_grid_height);
            // Mark all cells dirty to ensure proper rendering after resize.
            data->world->markAllCellsDirty();
        }

        // Update the label.
        char buf[16];
        snprintf(buf, sizeof(buf), "%.0f", static_cast<double>(value));
        lv_label_set_text(data->associated_label, buf);
    }
}

void SimulatorUI::pressureScaleWorldBSliderEventCb(lv_event_t* e)
{
    lv_obj_t* slider = static_cast<lv_obj_t*>(lv_event_get_target(e));
    CallbackData* data = static_cast<CallbackData*>(lv_event_get_user_data(e));
    if (lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED && data) {
        int32_t value = lv_slider_get_value(slider);
        double pressure_scale = value / 100.0;
        if (data->world) {
            // Only apply pressure scale changes to WorldB (RulesB).
            if (data->world->getWorldType() != WorldType::RulesB) {
                spdlog::debug(
                    "Pressure scale slider (WorldB) only affects WorldB (RulesB) - current world "
                    "is WorldA (RulesA)");
                // Still update the label to show the value for consistency.
                char buf[16];
                snprintf(buf, sizeof(buf), "%.1f", pressure_scale);
                lv_label_set_text(data->associated_label, buf);
                return;
            }
            data->world->setPressureScale(pressure_scale);
            spdlog::info("Pressure scale (WorldB) slider changed to: {:.1f}", pressure_scale);
        }
        char buf[16];
        snprintf(buf, sizeof(buf), "%.1f", pressure_scale);
        lv_label_set_text(data->associated_label, buf);
    }
}

void SimulatorUI::quitBtnEventCb(lv_event_t* e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        // Take exit screenshot before quitting.
        takeExitScreenshot();

        // Exit the application.
        exit(0);
    }
}

void SimulatorUI::leftThrowBtnEventCb(lv_event_t* e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        CallbackData* data = static_cast<CallbackData*>(lv_event_get_user_data(e));
        if (data && data->world) {
            bool current_state = data->world->isLeftThrowEnabled();
            bool new_state = !current_state;
            data->world->setLeftThrowEnabled(new_state);
            const lv_obj_t* btn = static_cast<const lv_obj_t*>(lv_event_get_target(e));
            lv_obj_t* label = lv_obj_get_child(btn, 0);
            lv_label_set_text(label, new_state ? "Left Throw: On" : "Left Throw: Off");
        }
    }
}

void SimulatorUI::rightThrowBtnEventCb(lv_event_t* e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        CallbackData* data = static_cast<CallbackData*>(lv_event_get_user_data(e));
        if (data && data->world) {
            bool current_state = data->world->isRightThrowEnabled();
            bool new_state = !current_state;
            data->world->setRightThrowEnabled(new_state);
            const lv_obj_t* btn = static_cast<const lv_obj_t*>(lv_event_get_target(e));
            lv_obj_t* label = lv_obj_get_child(btn, 0);
            lv_label_set_text(label, new_state ? "Right Throw: On" : "Right Throw: Off");
        }
    }
}

void SimulatorUI::quadrantBtnEventCb(lv_event_t* e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        CallbackData* data = static_cast<CallbackData*>(lv_event_get_user_data(e));
        if (data && data->world) {
            bool current_state = data->world->isLowerRightQuadrantEnabled();
            bool new_state = !current_state;
            data->world->setLowerRightQuadrantEnabled(new_state);
            const lv_obj_t* btn = static_cast<const lv_obj_t*>(lv_event_get_target(e));
            lv_obj_t* label = lv_obj_get_child(btn, 0);
            lv_label_set_text(label, new_state ? "Quadrant: On" : "Quadrant: Off");
        }
    }
}

void SimulatorUI::rainSliderEventCb(lv_event_t* e)
{
    lv_obj_t* slider = static_cast<lv_obj_t*>(lv_event_get_target(e));
    CallbackData* data = static_cast<CallbackData*>(lv_event_get_user_data(e));
    if (lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED && data) {
        int32_t value = lv_slider_get_value(slider);
        double rain_rate = value * 1.0; // Map 0-100 to 0-100 drops per second.
        if (data->world) {
            data->world->setRainRate(rain_rate);
        }
        char buf[16];
        snprintf(buf, sizeof(buf), "%.0f/s", rain_rate);
        lv_label_set_text(data->associated_label, buf);
    }
}

void SimulatorUI::waterCohesionSliderEventCb(lv_event_t* e)
{
    lv_obj_t* slider = static_cast<lv_obj_t*>(lv_event_get_target(e));
    CallbackData* data = static_cast<CallbackData*>(lv_event_get_user_data(e));
    if (lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED && data) {
        int32_t value = lv_slider_get_value(slider);
        double cohesion = value / 1000.0; // Map 0-1000 to 0.0-1.0.

        // Update WorldA cohesion (legacy system).
        Cell::setCohesionStrength(cohesion);

        // Update WorldB water cohesion (pure-material system).
        setMaterialCohesion(MaterialType::WATER, cohesion);

        char buf[16];
        snprintf(buf, sizeof(buf), "%.3f", cohesion);
        lv_label_set_text(data->associated_label, buf);
    }
}

void SimulatorUI::waterViscositySliderEventCb(lv_event_t* e)
{
    lv_obj_t* slider = static_cast<lv_obj_t*>(lv_event_get_target(e));
    CallbackData* data = static_cast<CallbackData*>(lv_event_get_user_data(e));
    if (lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED && data) {
        int32_t value = lv_slider_get_value(slider);
        double viscosity = value / 1000.0; // Map 0-1000 to 0.0-1.0.
        Cell::setViscosityFactor(viscosity);
        char buf[16];
        snprintf(buf, sizeof(buf), "%.3f", viscosity);
        lv_label_set_text(data->associated_label, buf);
    }
}

void SimulatorUI::waterPressureThresholdSliderEventCb(lv_event_t* e)
{
    lv_obj_t* slider = static_cast<lv_obj_t*>(lv_event_get_target(e));
    CallbackData* data = static_cast<CallbackData*>(lv_event_get_user_data(e));
    if (lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED && data) {
        int32_t value = lv_slider_get_value(slider);
        double threshold = value / 100000.0; // Map 0-1000 to 0.0-0.01.
        if (data->world) {
            data->world->setWaterPressureThreshold(threshold);
        }
        char buf[16];
        snprintf(buf, sizeof(buf), "%.4f", threshold);
        lv_label_set_text(data->associated_label, buf);
    }
}

void SimulatorUI::waterBuoyancySliderEventCb(lv_event_t* e)
{
    lv_obj_t* slider = static_cast<lv_obj_t*>(lv_event_get_target(e));
    CallbackData* data = static_cast<CallbackData*>(lv_event_get_user_data(e));
    if (lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED && data) {
        int32_t value = lv_slider_get_value(slider);
        double buoyancy = value / 1000.0; // Map 0-1000 to 0.0-1.0.
        Cell::setBuoyancyStrength(buoyancy);
        char buf[16];
        snprintf(buf, sizeof(buf), "%.3f", buoyancy);
        lv_label_set_text(data->associated_label, buf);
    }
}

void SimulatorUI::hydrostaticPressureToggleEventCb(lv_event_t* e)
{
    if (lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED) {
        CallbackData* data = static_cast<CallbackData*>(lv_event_get_user_data(e));
        if (data && data->world) {
            lv_obj_t* switch_obj = static_cast<lv_obj_t*>(lv_event_get_target(e));
            bool enabled = lv_obj_has_state(switch_obj, LV_STATE_CHECKED);
            data->world->setHydrostaticPressureEnabled(enabled);
            spdlog::info("Hydrostatic pressure {}", enabled ? "enabled" : "disabled");
        }
    }
}

void SimulatorUI::dynamicPressureToggleEventCb(lv_event_t* e)
{
    if (lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED) {
        CallbackData* data = static_cast<CallbackData*>(lv_event_get_user_data(e));
        if (data && data->world) {
            lv_obj_t* switch_obj = static_cast<lv_obj_t*>(lv_event_get_target(e));
            bool enabled = lv_obj_has_state(switch_obj, LV_STATE_CHECKED);
            data->world->setDynamicPressureEnabled(enabled);
            spdlog::info("Dynamic pressure {}", enabled ? "enabled" : "disabled");
        }
    }
}

void SimulatorUI::pressureDiffusionToggleEventCb(lv_event_t* e)
{
    if (lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED) {
        CallbackData* data = static_cast<CallbackData*>(lv_event_get_user_data(e));
        if (data && data->world) {
            lv_obj_t* switch_obj = static_cast<lv_obj_t*>(lv_event_get_target(e));
            bool enabled = lv_obj_has_state(switch_obj, LV_STATE_CHECKED);
            data->world->setPressureDiffusionEnabled(enabled);
            spdlog::info("Pressure diffusion {}", enabled ? "enabled" : "disabled");
        }
    }
}

void SimulatorUI::airResistanceSliderEventCb(lv_event_t* e)
{
    lv_obj_t* slider = static_cast<lv_obj_t*>(lv_event_get_target(e));
    CallbackData* data = static_cast<CallbackData*>(lv_event_get_user_data(e));
    if (lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED && data) {
        int32_t value = lv_slider_get_value(slider);
        double air_resistance = value / 100.0; // Map 0-100 to 0.0-1.0.
        if (data->world) {
            data->world->setAirResistanceStrength(air_resistance);
        }
        char buf[16];
        snprintf(buf, sizeof(buf), "%.2f", air_resistance);
        lv_label_set_text(data->associated_label, buf);
    }
}

void SimulatorUI::hydrostaticPressureStrengthSliderEventCb(lv_event_t* e)
{
    lv_obj_t* slider = static_cast<lv_obj_t*>(lv_event_get_target(e));
    CallbackData* data = static_cast<CallbackData*>(lv_event_get_user_data(e));
    if (lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED && data) {
        int32_t value = lv_slider_get_value(slider);
        double strength = value / 100.0; // Map 0-300 to 0.0-3.0.
        if (data->world) {
            // Only apply to WorldB (RulesB).
            if (data->world->getWorldType() == WorldType::RulesB) {
                data->world->setHydrostaticPressureStrength(strength);
                spdlog::debug("Hydrostatic pressure strength set to {:.2f}", strength);
            }
        }
        char buf[16];
        snprintf(buf, sizeof(buf), "%.1f", strength);
        lv_label_set_text(data->associated_label, buf);
    }
}

void SimulatorUI::dynamicPressureStrengthSliderEventCb(lv_event_t* e)
{
    lv_obj_t* slider = static_cast<lv_obj_t*>(lv_event_get_target(e));
    CallbackData* data = static_cast<CallbackData*>(lv_event_get_user_data(e));
    if (lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED && data) {
        int32_t value = lv_slider_get_value(slider);
        double strength = value / 100.0; // Map 0-300 to 0.0-3.0.
        if (data->world) {
            // Only apply to WorldB (RulesB).
            if (data->world->getWorldType() == WorldType::RulesB) {
                data->world->setDynamicPressureStrength(strength);
                spdlog::info(
                    "Dynamic Strength slider changed to: {:.1f} (via old callback)", strength);
            }
        }
        char buf[16];
        snprintf(buf, sizeof(buf), "%.1f", strength);
        lv_label_set_text(data->associated_label, buf);
    }
}

// Get the directory containing the executable.
std::string get_executable_directory()
{
    char path[PATH_MAX];
    ssize_t count = readlink("/proc/self/exe", path, PATH_MAX);
    if (count == -1) {
        printf("Failed to get executable path\n");
        return "."; // Return current directory as fallback.
    }
    path[count] = '\0';
    char* dir = dirname(path);
    return std::string(dir);
}

// PNG writer using LODEPNG.
void write_png_file(const char* filename, const uint8_t* rgb_data, uint32_t width, uint32_t height)
{
    // Convert BGR to RGB since LVGL might be providing BGR data.
    std::vector<uint8_t> corrected_data(width * height * 3);
    for (uint32_t i = 0; i < width * height; i++) {
        uint32_t src_idx = i * 3;
        uint32_t dst_idx = i * 3;
        // Swap R and B channels (BGR -> RGB).
        corrected_data[dst_idx + 0] = rgb_data[src_idx + 2]; // R from B.
        corrected_data[dst_idx + 1] = rgb_data[src_idx + 1]; // G stays G.
        corrected_data[dst_idx + 2] = rgb_data[src_idx + 0]; // B from R.
    }

    unsigned char* png_data;
    size_t png_size;

    // Encode corrected RGB data to PNG.
    unsigned error = lodepng_encode24(&png_data, &png_size, corrected_data.data(), width, height);

    if (error) {
        printf("PNG encoding error %u: %s\n", error, lodepng_error_text(error));
        return;
    }

    // Write PNG data to file.
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

void SimulatorUI::screenshotBtnEventCb(lv_event_t* e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        // Generate ISO8601 timestamp.
        time_t now = time(nullptr);
        struct tm* timeinfo = localtime(&now);
        char timestamp[32];
        strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%S", timeinfo);

        // Get executable directory and create filename.
        std::string exe_dir = get_executable_directory();
        char filename[512];
        snprintf(filename, sizeof(filename), "%s/screenshot-%s.png", exe_dir.c_str(), timestamp);

        CallbackData* data = static_cast<CallbackData*>(lv_event_get_user_data(e));
        if (!data || !data->ui || !data->ui->screen_) {
            printf("Screenshot failed: Invalid UI data\n");
            return;
        }

        // Take screenshot using LVGL snapshot.
        lv_draw_buf_t* snapshot = lv_snapshot_take(data->ui->screen_, LV_COLOR_FORMAT_RGB888);
        if (!snapshot) {
            printf("Failed to take LVGL snapshot\n");
            return;
        }

        // Get snapshot dimensions and data.
        uint32_t width = snapshot->header.w;
        uint32_t height = snapshot->header.h;
        uint8_t* rgb_data = static_cast<uint8_t*>(snapshot->data);

        printf("Captured snapshot: %dx%d pixels\n", width, height);

        // Save as PNG file in the same directory as the binary.
        write_png_file(filename, rgb_data, width, height);

        // Clean up the snapshot.
        lv_draw_buf_destroy(snapshot);

        // Print UI layout info for debugging overlap issues.
        lv_area_t screen_area;
        lv_obj_get_coords(data->ui->screen_, &screen_area);
        printf(
            "UI Layout Info - Screen area: x1=%d, y1=%d, x2=%d, y2=%d (width=%d, height=%d)\n",
            screen_area.x1,
            screen_area.y1,
            screen_area.x2,
            screen_area.y2,
            lv_area_get_width(&screen_area),
            lv_area_get_height(&screen_area));
    }
}

void SimulatorUI::printAsciiBtnEventCb(lv_event_t* e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        CallbackData* data = static_cast<CallbackData*>(lv_event_get_user_data(e));
        if (data && data->world) {
            std::string ascii_diagram = data->world->toAsciiDiagram();
            spdlog::info("Current world state (ASCII diagram):\n{}", ascii_diagram);
        }
        else {
            spdlog::warn("Print ASCII button clicked but no world available");
        }
    }
}

void SimulatorUI::takeExitScreenshot()
{
    // Get current screen.
    lv_obj_t* screen = lv_scr_act();
    if (!screen) {
        printf("No active screen found for exit screenshot\n");
        return;
    }

    // Take screenshot using LVGL snapshot.
    lv_draw_buf_t* snapshot = lv_snapshot_take(screen, LV_COLOR_FORMAT_RGB888);
    if (!snapshot) {
        printf("Failed to take exit screenshot\n");
        return;
    }

    // Get executable directory and create filename.
    const std::string exec_dir = get_executable_directory();
    const std::string filename = exec_dir + "/screenshot-last-exit.png";

    // Get buffer data and dimensions.
    const uint8_t* rgb_data = static_cast<const uint8_t*>(snapshot->data);
    const uint32_t width = snapshot->header.w;
    const uint32_t height = snapshot->header.h;

    // Save PNG file.
    write_png_file(filename.c_str(), rgb_data, width, height);

    // Clean up.
    lv_draw_buf_destroy(snapshot);

    printf("Exit screenshot saved as: %s\n", filename.c_str());
}

// Time reversal callback implementations.
void SimulatorUI::timeReversalToggleBtnEventCb(lv_event_t* e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        CallbackData* data = static_cast<CallbackData*>(lv_event_get_user_data(e));
        if (data && data->world) {
            bool current_state = data->world->isTimeReversalEnabled();
            bool new_state = !current_state;
            data->world->enableTimeReversal(new_state);
            const lv_obj_t* btn = static_cast<const lv_obj_t*>(lv_event_get_target(e));
            lv_obj_t* label = lv_obj_get_child(btn, 0);
            lv_label_set_text(label, new_state ? "Time History: On" : "Time History: Off");

            if (!new_state) {
                // Clear history when disabled.
                data->world->clearHistory();
            }
        }
    }
}

void SimulatorUI::backwardBtnEventCb(lv_event_t* e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        CallbackData* data = static_cast<CallbackData*>(lv_event_get_user_data(e));
        if (data && data->world) {
            if (data->world->canGoBackward()) {
                data->world->goBackward();
                printf("Went backward in time. History size: %zu\n", data->world->getHistorySize());
            }
            else {
                printf("Cannot go backward - no history available\n");
            }
        }
    }
}

void SimulatorUI::forwardBtnEventCb(lv_event_t* e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        CallbackData* data = static_cast<CallbackData*>(lv_event_get_user_data(e));
        if (data && data->world) {
            if (data->world->canGoForward()) {
                data->world->goForward();
                printf("Went forward in time. History size: %zu\n", data->world->getHistorySize());
            }
            else {
                printf("Cannot go forward - already at most recent state\n");
            }
        }
    }
}

void SimulatorUI::worldTypeButtonMatrixEventCb(lv_event_t* e)
{
    if (lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED) {
        CallbackData* data = static_cast<CallbackData*>(lv_event_get_user_data(e));
        if (data && data->ui) {
            lv_obj_t* btnm = static_cast<lv_obj_t*>(lv_event_get_target(e));
            uint32_t selected = lv_buttonmatrix_get_selected_button(btnm);

            // Convert button selection to WorldType.
            WorldType newType = (selected == 0) ? WorldType::RulesA : WorldType::RulesB;

            printf(
                "World type switch requested: %s\n",
                newType == WorldType::RulesA ? "WorldA (RulesA)" : "WorldB (RulesB)");

            // Request the world switch from the simulation manager.
            data->ui->requestWorldTypeSwitch(newType);
        }
    }
}

void SimulatorUI::requestWorldTypeSwitch(WorldType newType)
{
    if (!manager_) {
        spdlog::error("Cannot switch world type - no simulation manager set");
        return;
    }

    spdlog::info("Requesting world type switch to {}", getWorldTypeName(newType));

    if (manager_->switchWorldType(newType)) {
        // Update UI to reflect the switch.
        updateWorldTypeButtonMatrix(newType);
        updateScenarioDropdown(); // Update scenarios for the new world type.
        spdlog::info("World type switch request completed successfully");
    }
    else {
        spdlog::error("World type switch request failed");
    }
}

void SimulatorUI::updateWorldTypeButtonMatrix(WorldType currentType)
{
    if (world_type_btnm_) {
        uint32_t buttonIndex = (currentType == WorldType::RulesA) ? 0 : 1;
        lv_buttonmatrix_set_selected_button(world_type_btnm_, buttonIndex);
    }
}

void SimulatorUI::updateScenarioDropdown()
{
    SPARKLE_ASSERT(
        scenario_dropdown_, "updateScenarioDropdown called before scenario_dropdown_ is created");

    // Get current world type from manager.
    WorldType currentWorldType = WorldType::RulesB;
    if (manager_ && manager_->getWorld()) {
        currentWorldType = manager_->getWorld()->getWorldType();
    }
    else {
        // During initialization, use the default world type (WorldB).
        // This matches the initial button matrix selection (line 224 and 252).
        spdlog::debug("updateScenarioDropdown: manager_ is null, using default WorldType::RulesB");
    }

    // Get scenarios from registry filtered by current world type.
    auto& registry = ScenarioRegistry::getInstance();
    bool isWorldB = (currentWorldType == WorldType::RulesB);
    auto scenarioIds = registry.getScenariosForWorldType(isWorldB);

    // Clear existing options.
    lv_dropdown_clear_options(scenario_dropdown_);

    // Add scenarios to dropdown.
    for (const auto& id : scenarioIds) {
        auto* scenario = registry.getScenario(id);
        if (scenario) {
            const auto& metadata = scenario->getMetadata();
            lv_dropdown_add_option(scenario_dropdown_, metadata.name.c_str(), LV_DROPDOWN_POS_LAST);
        }
    }

    // If no scenarios available, add a placeholder.
    if (scenarioIds.empty()) {
        lv_dropdown_add_option(scenario_dropdown_, "(No scenarios)", LV_DROPDOWN_POS_LAST);
    }

    // Set the selection to Sandbox by default (or current scenario if we can detect it)
    // For now, find "Sandbox" in the list and select it
    uint16_t sandboxIndex = 0;
    for (size_t i = 0; i < scenarioIds.size(); i++) {
        auto* scenario = registry.getScenario(scenarioIds[i]);
        if (scenario && scenario->getMetadata().name == "Sandbox") {
            sandboxIndex = i;
            break;
        }
    }
    lv_dropdown_set_selected(scenario_dropdown_, sandboxIndex);
}

void SimulatorUI::onScenarioChanged(lv_event_t* e)
{
    CallbackData* data = static_cast<CallbackData*>(lv_event_get_user_data(e));
    if (!data) {
        spdlog::error("onScenarioChanged: null callback data");
        return;
    }

    SimulatorUI* ui = data->ui;
    if (!ui || !ui->scenario_dropdown_ || !ui->manager_) {
        return;
    }

    uint16_t selected = lv_dropdown_get_selected(ui->scenario_dropdown_);

    // Get current world type.
    WorldType currentWorldType = WorldType::RulesB;
    if (ui->manager_->getWorld()) {
        currentWorldType = ui->manager_->getWorld()->getWorldType();
    }

    // Get the selected scenario.
    auto& registry = ScenarioRegistry::getInstance();
    bool isWorldB = (currentWorldType == WorldType::RulesB);
    auto scenarioIds = registry.getScenariosForWorldType(isWorldB);

    if (selected < scenarioIds.size()) {
        const auto& id = scenarioIds[selected];
        auto* scenario = registry.getScenario(id);

        if (scenario) {
            const auto& metadata = scenario->getMetadata();
            spdlog::info("Loading scenario: {}", metadata.name);

            // Pause the simulation before changing scenarios to avoid race conditions
            bool wasRunning = !ui->is_paused_;
            if (wasRunning) {
                ui->is_paused_ = true;
                if (ui->pause_label_) {
                    lv_label_set_text(ui->pause_label_, "Resume");
                }
            }

            // Check if scenario requires specific world dimensions
            if (metadata.requiredWidth > 0 && metadata.requiredHeight > 0) {
                spdlog::info(
                    "Scenario requires {}x{} world dimensions",
                    metadata.requiredWidth,
                    metadata.requiredHeight);
                ui->manager_->resizeWorldIfNeeded(metadata.requiredWidth, metadata.requiredHeight);
            }
            else {
                // No specific dimensions required - restore defaults
                spdlog::info(
                    "Scenario has no dimension requirements - restoring default dimensions");
                ui->manager_->resizeWorldIfNeeded(0, 0);
            }

            // Create WorldSetup from the scenario.
            auto setup = scenario->createWorldSetup();

            // Apply the scenario to the world.
            // Note: This will call setup() which resets the world
            if (ui->manager_->getWorld()) {
                ui->manager_->getWorld()->setWorldSetup(std::move(setup));
            }

            // Update UI controls to reflect the new world state.
            ui->populateFromWorld();

            // Resume if it was running before
            if (wasRunning) {
                ui->is_paused_ = false;
                if (ui->pause_label_) {
                    lv_label_set_text(ui->pause_label_, "Pause");
                }
            }
        }
    }
}

void SimulatorUI::cohesionForceStrengthSliderEventCb(lv_event_t* e)
{
    lv_obj_t* slider = static_cast<lv_obj_t*>(lv_event_get_target(e));
    CallbackData* data = static_cast<CallbackData*>(lv_event_get_user_data(e));
    if (lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED && data) {
        int32_t value = lv_slider_get_value(slider);
        double strength = value / 100.0; // Range 0.0 to 20.0.
        if (data->world) {
            data->world->setCohesionComForceStrength(strength);
        }
        char buf[16];
        snprintf(buf, sizeof(buf), "%.1f", strength);
        lv_label_set_text(data->associated_label, buf);
    }
}

void SimulatorUI::adhesionStrengthSliderEventCb(lv_event_t* e)
{
    lv_obj_t* slider = static_cast<lv_obj_t*>(lv_event_get_target(e));
    CallbackData* data = static_cast<CallbackData*>(lv_event_get_user_data(e));
    if (lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED && data) {
        int32_t value = lv_slider_get_value(slider);
        double strength = value / 100.0; // Range 0.0 to 10.0.
        if (data->world) {
            data->world->setAdhesionStrength(strength);
        }
        char buf[16];
        snprintf(buf, sizeof(buf), "%.1f", strength);
        lv_label_set_text(data->associated_label, buf);
    }
}

void SimulatorUI::viscosityStrengthSliderEventCb(lv_event_t* e)
{
    lv_obj_t* slider = static_cast<lv_obj_t*>(lv_event_get_target(e));
    CallbackData* data = static_cast<CallbackData*>(lv_event_get_user_data(e));
    if (lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED && data) {
        int32_t value = lv_slider_get_value(slider);
        double strength = value / 100.0; // Range 0.0 to 2.0.
        if (data->world) {
            data->world->setViscosityStrength(strength);
        }
        char buf[16];
        snprintf(buf, sizeof(buf), "%.1f", strength);
        lv_label_set_text(data->associated_label, buf);
    }
}

void SimulatorUI::comCohesionRangeSliderEventCb(lv_event_t* e)
{
    lv_obj_t* slider = static_cast<lv_obj_t*>(lv_event_get_target(e));
    CallbackData* data = static_cast<CallbackData*>(lv_event_get_user_data(e));
    if (lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED && data) {
        int32_t value = lv_slider_get_value(slider);
        uint32_t range = static_cast<uint32_t>(value); // Range 1 to 5.
        if (data->world) {
            data->world->setCOMCohesionRange(range);
        }
        char buf[16];
        snprintf(buf, sizeof(buf), "%u", range);
        lv_label_set_text(data->associated_label, buf);
    }
}

void SimulatorUI::frictionStrengthSliderEventCb(lv_event_t* e)
{
    lv_obj_t* slider = static_cast<lv_obj_t*>(lv_event_get_target(e));
    CallbackData* data = static_cast<CallbackData*>(lv_event_get_user_data(e));
    if (lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED && data) {
        int32_t value = lv_slider_get_value(slider);
        double strength = value / 1.0; // Range 0 to 10.
        spdlog::info("Friction strength changed to {:.1f}", strength);
        if (data->world) {
            data->world->setFrictionStrength(strength);
        }
        char buf[16];
        snprintf(buf, sizeof(buf), "%.1f", strength);
        lv_label_set_text(data->associated_label, buf);
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
