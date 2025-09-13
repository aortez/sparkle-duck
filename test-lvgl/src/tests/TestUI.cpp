#include "TestUI.h"
#include "../WorldInterface.h"
#include "visual_test_runner.h"
#include "lvgl/lvgl.h"
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <thread>
#include <chrono>

TestUI::TestUI(lv_obj_t* screen, const std::string& test_name)
    : world_(nullptr),
      screen_(screen),
      draw_area_(nullptr),
      test_label_(nullptr),
      start_button_(nullptr),
      next_button_(nullptr),
      step_button_(nullptr),
      run10_button_(nullptr),
      button_status_label_(nullptr),
      test_name_(test_name)
{}

TestUI::~TestUI() = default;

void TestUI::setWorld(WorldInterface* world)
{
    world_ = world;
}

void TestUI::initialize()
{
    createDrawArea();
    createLabels();
    createButtons();
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
    // Create test name label with better formatting.
    test_label_ = lv_label_create(screen_);
    
    // Extract just the test name without the full class path for better readability.
    std::string short_test_name = test_name_;
    size_t dot_pos = short_test_name.find_last_of('.');
    if (dot_pos != std::string::npos) {
        short_test_name = short_test_name.substr(dot_pos + 1);
    }
    
    std::string initial_text = short_test_name;
    lv_label_set_text(test_label_, initial_text.c_str());
    
    // Position the label and set width constraints.
    lv_obj_align(test_label_, LV_ALIGN_TOP_LEFT, DRAW_AREA_SIZE + 20, 10);
    lv_obj_set_width(test_label_, 180); // Constrain width to prevent overlap.
    lv_label_set_long_mode(test_label_, LV_LABEL_LONG_WRAP); // Enable text wrapping.
    
    // Set a smaller font size for better fitting.
    lv_obj_set_style_text_font(test_label_, &lv_font_montserrat_12, 0);
}

void TestUI::createButtons()
{
    // Create Start button.
    start_button_ = lv_btn_create(screen_);
    lv_obj_set_size(start_button_, 80, 40);
    lv_obj_align(start_button_, LV_ALIGN_TOP_LEFT, DRAW_AREA_SIZE + 20, 60);
    lv_obj_add_event_cb(start_button_, startButtonEventHandler, LV_EVENT_CLICKED, this);
    
    lv_obj_t* start_label = lv_label_create(start_button_);
    lv_label_set_text(start_label, "Start");
    lv_obj_center(start_label);
    
    // Create Next button.
    next_button_ = lv_btn_create(screen_);
    lv_obj_set_size(next_button_, 80, 40);
    lv_obj_align(next_button_, LV_ALIGN_TOP_LEFT, DRAW_AREA_SIZE + 110, 60);
    lv_obj_add_event_cb(next_button_, nextButtonEventHandler, LV_EVENT_CLICKED, this);
    lv_obj_add_state(next_button_, LV_STATE_DISABLED); // Start disabled.
    
    lv_obj_t* next_label = lv_label_create(next_button_);
    lv_label_set_text(next_label, "Next");
    lv_obj_center(next_label);
    
    // Create Step button.
    step_button_ = lv_btn_create(screen_);
    lv_obj_set_size(step_button_, 80, 40);
    lv_obj_align(step_button_, LV_ALIGN_TOP_LEFT, DRAW_AREA_SIZE + 20, 110);
    lv_obj_add_event_cb(step_button_, stepButtonEventHandler, LV_EVENT_CLICKED, this);
    lv_obj_add_state(step_button_, LV_STATE_DISABLED); // Start disabled.
    
    lv_obj_t* step_label = lv_label_create(step_button_);
    lv_label_set_text(step_label, "Step");
    lv_obj_center(step_label);
    
    // Create Run10 button.
    run10_button_ = lv_btn_create(screen_);
    lv_obj_set_size(run10_button_, 80, 40);
    lv_obj_align(run10_button_, LV_ALIGN_TOP_LEFT, DRAW_AREA_SIZE + 110, 110);
    lv_obj_add_event_cb(run10_button_, run10ButtonEventHandler, LV_EVENT_CLICKED, this);
    lv_obj_add_state(run10_button_, LV_STATE_DISABLED); // Start disabled.
    
    lv_obj_t* run10_label = lv_label_create(run10_button_);
    lv_label_set_text(run10_label, "Run10");
    lv_obj_center(run10_label);
    
    // Create button status label.
    button_status_label_ = lv_label_create(screen_);
    lv_label_set_text(button_status_label_, "Press Start to begin test");
    lv_obj_align(button_status_label_, LV_ALIGN_TOP_LEFT, DRAW_AREA_SIZE + 20, 160);
    lv_obj_set_width(button_status_label_, 180);
    lv_label_set_long_mode(button_status_label_, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(button_status_label_, &lv_font_montserrat_12, 0);
}

void TestUI::updateTestLabel(const std::string& status)
{
    if (test_label_) {
        // Create a more compact status display.
        std::string short_test_name = test_name_;
        size_t dot_pos = short_test_name.find_last_of('.');
        if (dot_pos != std::string::npos) {
            short_test_name = short_test_name.substr(dot_pos + 1);
        }
        
        // Truncate very long status messages.
        std::string display_status = status;
        if (display_status.length() > 40) {
            display_status = display_status.substr(0, 37) + "...";
        }
        
        std::string text = short_test_name + "\n" + display_status;
        lv_label_set_text(test_label_, text.c_str());
    }
}



void TestUI::enableStartButton()
{
    if (start_button_) {
        lv_obj_clear_state(start_button_, LV_STATE_DISABLED);
    }
}

void TestUI::enableNextButton()
{
    if (next_button_) {
        lv_obj_clear_state(next_button_, LV_STATE_DISABLED);
    }
}

void TestUI::disableNextButton()
{
    if (next_button_) {
        lv_obj_add_state(next_button_, LV_STATE_DISABLED);
    }
}


void TestUI::enableStepButton()
{
    if (step_button_) {
        lv_obj_clear_state(step_button_, LV_STATE_DISABLED);
    }
}

void TestUI::disableStepButton()
{
    if (step_button_) {
        lv_obj_add_state(step_button_, LV_STATE_DISABLED);
    }
}

void TestUI::updateButtonStatus(const std::string& status)
{
    if (button_status_label_) {
        lv_label_set_text(button_status_label_, status.c_str());
    }
}

void TestUI::startButtonEventHandler(lv_event_t* e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        TestUI* ui = static_cast<TestUI*>(lv_event_get_user_data(e));
        if (ui) {
            spdlog::info("[UI] Start button clicked");
            
            ui->start_pressed_.store(true);
            
            if (ui->restart_mode_enabled_) {
                ui->restart_requested_.store(true);
                ui->updateButtonStatus("Restarting test...");
                // Keep start button enabled for repeated restarts.
            } else {
                ui->updateButtonStatus("Test started!");
                // Disable start button after first press in normal mode.
                lv_obj_add_state(ui->start_button_, LV_STATE_DISABLED);
            }
        }
    }
}

void TestUI::nextButtonEventHandler(lv_event_t* e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        TestUI* ui = static_cast<TestUI*>(lv_event_get_user_data(e));
        if (ui) {
            spdlog::info("[UI] Next button clicked!");
            ui->next_pressed_.store(true);
            ui->updateButtonStatus("Continuing...");
        }
    }
}

void TestUI::stepButtonEventHandler(lv_event_t* e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        TestUI* ui = static_cast<TestUI*>(lv_event_get_user_data(e));
        if (ui) {
            spdlog::info("[UI] Step button clicked! (step_mode={})", ui->step_mode_enabled_);
            
            // Always signal that step was pressed.
            ui->step_pressed_.store(true);
            
            // The test framework will handle the actual stepping.
            spdlog::info("[UI] Step button press signaled to test framework");
        }
    }
}

void TestUI::run10ButtonEventHandler(lv_event_t* e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        TestUI* ui = static_cast<TestUI*>(lv_event_get_user_data(e));
        if (ui && ui->world_) {
            ui->updateButtonStatus("Running 10 steps...");
            
            auto& coordinator = VisualTestCoordinator::getInstance();
            
            // Run 10 simulation steps with thread-safe drawing.
            for (int i = 0; i < 10; ++i) {
                ui->world_->advanceTime(0.016);
                
                // Thread-safe drawing.
                coordinator.postTaskSync([ui] {
                    ui->world_->draw();
                });
                
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
            
            ui->updateButtonStatus("10 steps completed!");
        }
    }
} 
