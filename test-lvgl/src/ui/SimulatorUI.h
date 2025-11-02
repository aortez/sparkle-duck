#pragma once

#include "../core/World.h"
#include "MaterialPicker.h"
#include "lvgl/lvgl.h"
#include <memory>
#include <optional>
#include <vector>

// Forward declarations
class EventRouter;
class UIUpdateConsumer;
struct UIUpdateEvent;
enum class WorldType;

class SimulatorUI {
public:
    // Callback struct for LVGL callbacks.
    struct CallbackData {
        SimulatorUI* ui;
        lv_obj_t* associated_label; // For sliders that need to update labels.
    };

    SimulatorUI(lv_obj_t* screen, EventRouter* eventRouter);
    ~SimulatorUI();

    // Require EventRouter - UI must communicate via events only.
    EventRouter* getEventRouter() const { return event_router_; }

    // UI update methods.
    void updateMassLabel(double totalMass);
    void updateFPSLabel(uint32_t fps);
    void updateDebugButton();
    void updateTimescaleSlider(double timescale);
    void updateElasticitySlider(double elasticity);
    void applyUpdate(const UIUpdateEvent& update);

    // Frame limiting control.
    bool isFrameLimitingEnabled() const { return frame_limiting_enabled_; }

    // Getters for UI elements that main might need.
    lv_obj_t* getDrawArea() const { return draw_area_; }

    // Initialize the UI after world is fully constructed.
    void initialize();

    // Populate UI controls with values from update event.
    void populateFromUpdate(const UIUpdateEvent& update);

    // Static function to take exit screenshot.
    static void takeExitScreenshot();

private:
    EventRouter* event_router_; // Event routing system (send user interactions).

    // Last world state from UIUpdateEvent (for comparison and rendering).
    std::optional<World> lastWorldState_;

    lv_obj_t* screen_;
    lv_obj_t* draw_area_;
    lv_obj_t* mass_label_;
    lv_obj_t* fps_label_;
    lv_obj_t* pause_label_;
    lv_obj_t* pause_btn_ = nullptr; // Store pause button reference for state sync.
    lv_obj_t* debug_btn_ = nullptr;
    lv_obj_t* scenario_dropdown_ = nullptr;
    lv_obj_t* left_throw_label_ = nullptr;
    lv_obj_t* right_throw_label_ = nullptr;

    // Pressure control switches.
    lv_obj_t* hydrostatic_switch_ = nullptr;
    lv_obj_t* dynamic_switch_ = nullptr;
    lv_obj_t* diffusion_switch_ = nullptr;

    // Physics control switches.
    lv_obj_t* cohesion_switch_ = nullptr;
    lv_obj_t* adhesion_switch_ = nullptr;

    // Slider references for UI sync after scenario changes.
    lv_obj_t* pressure_scale_slider_ = nullptr;
    lv_obj_t* pressure_scale_label_ = nullptr;
    lv_obj_t* pressure_scale_worldb_slider_ = nullptr;
    lv_obj_t* pressure_scale_worldb_label_ = nullptr;
    lv_obj_t* hydrostatic_strength_slider_ = nullptr;
    lv_obj_t* hydrostatic_strength_label_ = nullptr;
    lv_obj_t* dynamic_strength_slider_ = nullptr;
    lv_obj_t* dynamic_strength_label_ = nullptr;
    lv_obj_t* gravity_button_ = nullptr; // Gravity is a toggle button
    lv_obj_t* gravity_label_ = nullptr;
    lv_obj_t* timescale_slider_ = nullptr; // Timescale slider object
    lv_obj_t* timescale_label_ = nullptr;  // Value label for timescale slider
    lv_obj_t* elasticity_slider_ = nullptr;
    lv_obj_t* elasticity_label_ = nullptr;
    lv_obj_t* air_resistance_slider_ = nullptr;
    lv_obj_t* air_resistance_label_ = nullptr;
    lv_obj_t* cohesion_force_slider_ = nullptr;
    lv_obj_t* friction_strength_slider_ = nullptr;
    lv_obj_t* cohesion_force_label_ = nullptr;

    // Material picker UI.
    std::unique_ptr<MaterialPicker> material_picker_;

    // UI state.
    double timescale_;
    bool is_paused_;
    bool frame_limiting_enabled_; // Control frame rate limiting.

    // Mouse interaction mode tracking.
    enum class InteractionMode {
        NONE,      // No active interaction.
        GRAB_MODE, // Dragging existing material (current behavior).
        PAINT_MODE // Painting new material along path.
    };
    InteractionMode interaction_mode_;
    MaterialType paint_material_; // Material type for paint mode.

    // Layout dimensions.
    static constexpr int CONTROL_WIDTH = 200;
    static constexpr int DRAW_AREA_SIZE = 850;
    static constexpr int LEFT_COLUMN_X = DRAW_AREA_SIZE + 10;  // First column: scenarios + materials.
    static constexpr int RIGHT_COLUMN_X = LEFT_COLUMN_X + CONTROL_WIDTH + 10;  // Second column: controls.

    // Storage for callback data to keep them alive.
    std::vector<std::unique_ptr<CallbackData>> callback_data_storage_;

    // Push-based UI update system.
    std::unique_ptr<UIUpdateConsumer> updateConsumer_;
    lv_timer_t* updateTimer_ = nullptr;

    // Private methods for creating UI elements.
    void createDrawArea();
    void createLabels();
    void createScenarioDropdown();
    void createMaterialPicker();
    void createControlButtons();
    void createSliders();

    // Static event callback methods.
    static void drawAreaEventCb(lv_event_t* e);
    static void pauseBtnEventCb(lv_event_t* e);
    static void timescaleSliderEventCb(lv_event_t* e);
    static void cellSizeSliderEventCb(lv_event_t* e);
    static void resetBtnEventCb(lv_event_t* e);
    static void debugBtnEventCb(lv_event_t* e);
    static void pressureSystemDropdownEventCb(lv_event_t* e);
    static void gravityBtnEventCb(lv_event_t* e);
    static void cohesionBtnEventCb(lv_event_t* e);
    static void cohesionForceBtnEventCb(lv_event_t* e);
    static void adhesionBtnEventCb(lv_event_t* e);
    static void elasticitySliderEventCb(lv_event_t* e);
    static void fragmentationSliderEventCb(lv_event_t* e);
    static void pressureScaleSliderEventCb(lv_event_t* e);
    static void pressureScaleWorldBSliderEventCb(lv_event_t* e);
    static void quitBtnEventCb(lv_event_t* e);
    static void frameLimitBtnEventCb(lv_event_t* e);

    // Water physics sliders.
    static void waterCohesionSliderEventCb(lv_event_t* e);
    static void waterViscositySliderEventCb(lv_event_t* e);
    static void waterPressureThresholdSliderEventCb(lv_event_t* e);
    static void waterBuoyancySliderEventCb(lv_event_t* e);

    // Pressure system toggles.
    static void hydrostaticPressureToggleEventCb(lv_event_t* e);
    static void dynamicPressureToggleEventCb(lv_event_t* e);
    static void pressureDiffusionToggleEventCb(lv_event_t* e);
    static void airResistanceSliderEventCb(lv_event_t* e);

    // World pressure strength sliders.
    static void hydrostaticPressureStrengthSliderEventCb(lv_event_t* e);
    static void dynamicPressureStrengthSliderEventCb(lv_event_t* e);

    // WorldSetup control buttons.
    static void leftThrowBtnEventCb(lv_event_t* e);
    static void rightThrowBtnEventCb(lv_event_t* e);
    static void quadrantBtnEventCb(lv_event_t* e);
    static void rainSliderEventCb(lv_event_t* e);
    static void screenshotBtnEventCb(lv_event_t* e);
    static void printAsciiBtnEventCb(lv_event_t* e);

    // Time reversal control buttons.
    static void backwardBtnEventCb(lv_event_t* e);
    static void forwardBtnEventCb(lv_event_t* e);
    static void timeReversalToggleBtnEventCb(lv_event_t* e);

    // Cohesion strength sliders.
    static void cohesionForceStrengthSliderEventCb(lv_event_t* e);
    static void adhesionStrengthSliderEventCb(lv_event_t* e);
    static void viscosityStrengthSliderEventCb(lv_event_t* e);
    static void comCohesionRangeSliderEventCb(lv_event_t* e);
    static void frictionStrengthSliderEventCb(lv_event_t* e);

    // Push-based UI update timer callback.
    static void uiUpdateTimerCb(lv_timer_t* timer);

    // Helper to create callback data.
    CallbackData* createCallbackData(lv_obj_t* label = nullptr);

    // TODO: Scenario dropdown methods.
    // void updateScenarioDropdown();
    // static void onScenarioChanged(lv_event_t* e);
};
