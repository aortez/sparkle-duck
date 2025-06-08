#pragma once

#include "lvgl/lvgl.h"
#include <memory>
#include <vector>

// Forward declarations
class World;

class SimulatorUI {
public:
    // Callback struct to pass both UI and World pointers
    struct CallbackData {
        SimulatorUI* ui;
        World* world;
        lv_obj_t* associated_label; // For sliders that need to update labels
    };

    SimulatorUI(lv_obj_t* screen);
    ~SimulatorUI();

    // Set the world after UI creation
    void setWorld(World* world);
    World* getWorld() const { return world_; }

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
    World* world_;
    lv_obj_t* screen_;
    lv_obj_t* draw_area_;
    lv_obj_t* mass_label_;
    lv_obj_t* fps_label_;
    lv_obj_t* pause_label_;

    // UI state
    double timescale_;
    bool is_paused_;

    // Control column dimensions
    static constexpr int CONTROL_WIDTH = 200;
    static constexpr int DRAW_AREA_SIZE = 850;

    // Storage for callback data to keep them alive
    std::vector<std::unique_ptr<CallbackData>> callback_data_storage_;

    // Private methods for creating UI elements
    void createDrawArea();
    void createLabels();
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
    static void forceBtnEventCb(lv_event_t* e);
    static void gravityBtnEventCb(lv_event_t* e);
    static void elasticitySliderEventCb(lv_event_t* e);
    static void fragmentationSliderEventCb(lv_event_t* e);
    static void pressureSliderEventCb(lv_event_t* e);
    static void quitBtnEventCb(lv_event_t* e);

    // WorldSetup control buttons
    static void leftThrowBtnEventCb(lv_event_t* e);
    static void rightThrowBtnEventCb(lv_event_t* e);
    static void quadrantBtnEventCb(lv_event_t* e);
    static void rainSliderEventCb(lv_event_t* e);
    static void screenshotBtnEventCb(lv_event_t* e);

    // Helper to create callback data
    CallbackData* createCallbackData(lv_obj_t* label = nullptr);
};
