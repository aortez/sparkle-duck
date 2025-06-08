#pragma once

#include "lvgl/lvgl.h"
#include <memory>
#include <string>

// Forward declarations
class World;

class TestUI {
public:
    TestUI(lv_obj_t* screen, const std::string& test_name);
    ~TestUI();

    // Set the world after UI creation
    void setWorld(World* world);
    World* getWorld() const { return world_; }

    // UI update methods
    void updateTestLabel(const std::string& status);

    // Getters for UI elements
    lv_obj_t* getDrawArea() const { return draw_area_; }

    // Initialize the UI after world is fully constructed
    void initialize();

private:
    World* world_;
    lv_obj_t* screen_;
    lv_obj_t* draw_area_;
    lv_obj_t* test_label_;
    std::string test_name_;

    // Control column dimensions
    static constexpr int CONTROL_WIDTH = 200;
    static constexpr int DRAW_AREA_SIZE = 400; // Smaller for test UI

    // Private methods for creating UI elements
    void createDrawArea();
    void createLabels();
}; 
