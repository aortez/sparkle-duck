#pragma once

#include "lvgl/lvgl.h"
#include <memory>
#include <vector>

// Forward declarations
class WorldInterface;
class SimulationManager;
enum class WorldType;

class SimulatorUI {
public:
    // Callback struct to pass UI, World, and Manager pointers
    struct CallbackData {
        SimulatorUI* ui;
        WorldInterface* world;
        SimulationManager* manager;
        lv_obj_t* associated_label; // For sliders that need to update labels
    };

    SimulatorUI(lv_obj_t* screen);
    ~SimulatorUI();

    // Set the world and manager after UI creation
    void setWorld(WorldInterface* world);
    void setSimulationManager(SimulationManager* manager);
    WorldInterface* getWorld() const { return world_; }
    SimulationManager* getSimulationManager() const { return manager_; }

    // UI update methods
    void updateMassLabel(double totalMass);
    void updateFPSLabel(uint32_t fps);

    // Getters for UI elements that main might need
    lv_obj_t* getDrawArea() const { return draw_area_; }

    // Initialize the UI after world is fully constructed
    void initialize();

    // Static function to take exit screenshot
    static void takeExitScreenshot();

private:
    WorldInterface* world_;
    SimulationManager* manager_;  // Manager handles world switching
    lv_obj_t* screen_;
    lv_obj_t* draw_area_;
    lv_obj_t* mass_label_;
    lv_obj_t* fps_label_;
    lv_obj_t* pause_label_;
    lv_obj_t* world_type_btnm_;

    // UI state
    double timescale_;
    bool is_paused_;

    // Layout dimensions
    static constexpr int CONTROL_WIDTH = 200;
    static constexpr int DRAW_AREA_SIZE = 850;
    static constexpr int WORLD_TYPE_COLUMN_WIDTH = 150;
    static constexpr int WORLD_TYPE_COLUMN_X = DRAW_AREA_SIZE + 10;  // 10px margin from draw area
    static constexpr int MAIN_CONTROLS_X = WORLD_TYPE_COLUMN_X + WORLD_TYPE_COLUMN_WIDTH + 10;  // 10px margin from world type column

    // Storage for callback data to keep them alive
    std::vector<std::unique_ptr<CallbackData>> callback_data_storage_;

    // Private methods for creating UI elements
    void createDrawArea();
    void createLabels();
    void createWorldTypeColumn();
    void createControlButtons();
    void createSliders();
    void setupDrawAreaEvents();

    // Static event callback methods
    static void drawAreaEventCb(lv_event_t* e);
    static void pauseBtnEventCb(lv_event_t* e);
    static void timescaleSliderEventCb(lv_event_t* e);
    static void cellSizeSliderEventCb(lv_event_t* e);
    static void resetBtnEventCb(lv_event_t* e);
    static void debugBtnEventCb(lv_event_t* e);
    static void pressureSystemDropdownEventCb(lv_event_t* e);
    static void worldTypeButtonMatrixEventCb(lv_event_t* e);
    static void forceBtnEventCb(lv_event_t* e);
    static void gravityBtnEventCb(lv_event_t* e);
    static void elasticitySliderEventCb(lv_event_t* e);
    static void fragmentationSliderEventCb(lv_event_t* e);
    static void pressureScaleSliderEventCb(lv_event_t* e);
    static void quitBtnEventCb(lv_event_t* e);

    // Water physics sliders
    static void waterCohesionSliderEventCb(lv_event_t* e);
    static void waterViscositySliderEventCb(lv_event_t* e);
    static void waterPressureThresholdSliderEventCb(lv_event_t* e);
    static void waterBuoyancySliderEventCb(lv_event_t* e);

    // WorldSetup control buttons
    static void leftThrowBtnEventCb(lv_event_t* e);
    static void rightThrowBtnEventCb(lv_event_t* e);
    static void quadrantBtnEventCb(lv_event_t* e);
    static void rainSliderEventCb(lv_event_t* e);
    static void screenshotBtnEventCb(lv_event_t* e);

    // Time reversal control buttons
    static void backwardBtnEventCb(lv_event_t* e);
    static void forwardBtnEventCb(lv_event_t* e);
    static void timeReversalToggleBtnEventCb(lv_event_t* e);

    // Helper to create callback data
    CallbackData* createCallbackData(lv_obj_t* label = nullptr);
    
    // World type switching methods (delegates to manager)
    void requestWorldTypeSwitch(WorldType newType);
    void updateWorldTypeButtonMatrix(WorldType currentType);
    
};
