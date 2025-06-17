#pragma once

#include "lvgl/lvgl.h"
#include <memory>
#include <string>
#include <atomic>

// Forward declarations
class WorldInterface;

class TestUI {
public:
    TestUI(lv_obj_t* screen, const std::string& test_name);
    ~TestUI();

    // Set the world after UI creation
    void setWorld(WorldInterface* world);
    WorldInterface* getWorld() const { return world_; }

    // UI update methods
    void updateTestLabel(const std::string& status);

    // Getters for UI elements
    lv_obj_t* getDrawArea() const { return draw_area_; }

    // Initialize the UI after world is fully constructed
    void initialize();

    // Test control methods
    void waitForStart();          // Block until Start button is pressed
    void waitForNext();           // Block until Next button is pressed
    void waitForStep();           // Block until Step button is pressed
    void enableStartButton();     // Enable the Start button
    void enableNextButton();      // Enable the Next button
    void enableStepButton();      // Enable the Step button
    void disableNextButton();     // Disable the Next button
    void disableStepButton();     // Disable the Step button
    void updateButtonStatus(const std::string& status); // Update button area with status
    
    // Enhanced functionality
    void setStepMode(bool enabled) { step_mode_enabled_ = enabled; }
    bool isStepModeEnabled() const { return step_mode_enabled_; }
    void setRestartMode(bool enabled) { restart_mode_enabled_ = enabled; }
    bool isRestartModeEnabled() const { return restart_mode_enabled_; }
    
    // Button state access for external synchronization
    std::atomic<bool> start_pressed_{false};
    std::atomic<bool> next_pressed_{false};
    std::atomic<bool> step_pressed_{false};
    std::atomic<bool> restart_requested_{false};

private:
    WorldInterface* world_;
    lv_obj_t* screen_;
    lv_obj_t* draw_area_;
    lv_obj_t* test_label_;
    lv_obj_t* start_button_;
    lv_obj_t* next_button_;
    lv_obj_t* step_button_;
    lv_obj_t* run10_button_;
    lv_obj_t* button_status_label_;
    std::string test_name_;

    // Button state tracking moved to public section for external access
    
    // Enhanced UI modes
    bool step_mode_enabled_ = false;     // Step button advances simulation
    bool restart_mode_enabled_ = false;  // Start button acts as restart

public:
    // Control column dimensions
    static constexpr int CONTROL_WIDTH = 200;
    static constexpr int DRAW_AREA_SIZE = 400; // Smaller for test UI

private:

    // Private methods for creating UI elements
    void createDrawArea();
    void createLabels();
    void createButtons();

    // Button event handlers
    static void startButtonEventHandler(lv_event_t* e);
    static void nextButtonEventHandler(lv_event_t* e);
    static void stepButtonEventHandler(lv_event_t* e);
    static void run10ButtonEventHandler(lv_event_t* e);
}; 
