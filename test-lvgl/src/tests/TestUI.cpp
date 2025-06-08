#include "TestUI.h"
#include "../World.h"
#include "lvgl/lvgl.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

TestUI::TestUI(lv_obj_t* screen, const std::string& test_name)
    : world_(nullptr),
      screen_(screen),
      draw_area_(nullptr),
      test_label_(nullptr),
      test_name_(test_name)
{}

TestUI::~TestUI() = default;

void TestUI::setWorld(World* world)
{
    world_ = world;
}

void TestUI::initialize()
{
    createDrawArea();
    createLabels();
}

void TestUI::createDrawArea()
{
    draw_area_ = lv_obj_create(screen_);
    lv_obj_set_size(draw_area_, DRAW_AREA_SIZE, DRAW_AREA_SIZE);
    lv_obj_align(draw_area_, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_set_style_pad_all(draw_area_, 0, 0);
}

void TestUI::createLabels()
{
    // Create test name label with better formatting
    test_label_ = lv_label_create(screen_);
    
    // Extract just the test name without the full class path for better readability
    std::string short_test_name = test_name_;
    size_t dot_pos = short_test_name.find_last_of('.');
    if (dot_pos != std::string::npos) {
        short_test_name = short_test_name.substr(dot_pos + 1);
    }
    
    std::string initial_text = short_test_name;
    lv_label_set_text(test_label_, initial_text.c_str());
    
    // Position the label and set width constraints
    lv_obj_align(test_label_, LV_ALIGN_TOP_LEFT, DRAW_AREA_SIZE + 20, 10);
    lv_obj_set_width(test_label_, 180); // Constrain width to prevent overlap
    lv_label_set_long_mode(test_label_, LV_LABEL_LONG_WRAP); // Enable text wrapping
    
    // Set a smaller font size for better fitting
    lv_obj_set_style_text_font(test_label_, &lv_font_montserrat_12, 0);
}

void TestUI::updateTestLabel(const std::string& status)
{
    if (test_label_) {
        // Create a more compact status display
        std::string short_test_name = test_name_;
        size_t dot_pos = short_test_name.find_last_of('.');
        if (dot_pos != std::string::npos) {
            short_test_name = short_test_name.substr(dot_pos + 1);
        }
        
        // Truncate very long status messages
        std::string display_status = status;
        if (display_status.length() > 40) {
            display_status = display_status.substr(0, 37) + "...";
        }
        
        std::string text = short_test_name + "\n" + display_status;
        lv_label_set_text(test_label_, text.c_str());
    }
} 
