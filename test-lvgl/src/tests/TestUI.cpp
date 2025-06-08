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
    // Create test name label
    test_label_ = lv_label_create(screen_);
    std::string initial_text = "Test: " + test_name_;
    lv_label_set_text(test_label_, initial_text.c_str());
    lv_obj_align(test_label_, LV_ALIGN_TOP_LEFT, DRAW_AREA_SIZE + 20, 10);
}

void TestUI::updateTestLabel(const std::string& status)
{
    if (test_label_) {
        std::string text = "Test: " + test_name_ + "\nStatus: " + status;
        lv_label_set_text(test_label_, text.c_str());
    }
} 
