#pragma once

#include "lvgl/lvgl.h"
#include "lvgl/src/display/lv_display.h"
#include <gtest/gtest.h>
#include <chrono>
#include <thread>

/**
 * Base test fixture for tests that require LVGL functionality.
 * 
 * This fixture handles:
 * - LVGL initialization and cleanup
 * - Creating a minimal display (required for LVGL to create UI objects)
 * - Creating and managing a test screen
 * - Time advancement utilities for timer-based tests
 * 
 * Example usage:
 * ```cpp
 * class MyLVGLTest : public LVGLTestBase {
 * protected:
 *     void SetUp() override {
 *         LVGLTestBase::SetUp();
 *         // Your additional setup here
 *     }
 * };
 * ```
 */
class LVGLTestBase : public ::testing::Test {
public:
    /**
     * Creates a minimal display suitable for testing.
     * The display has a dummy flush callback that does nothing except
     * mark the display as ready (no actual rendering occurs).
     * 
     * @param width Display width in pixels (default: 100)
     * @param height Display height in pixels (default: 100)
     * @return Created display handle
     */
    static lv_display_t* createTestDisplay(int width = 100, int height = 100) {
        // Create display.
        lv_display_t* disp = lv_display_create(width, height);
        
        // Create buffer for the display (single buffered, minimal size).
        // Static to ensure it persists for the lifetime of the display.
        static lv_color_t buf[100 * 10];  // 10 rows buffer.
        lv_display_set_buffers(disp, buf, NULL, sizeof(buf), LV_DISPLAY_RENDER_MODE_PARTIAL);
        
        // Set a dummy flush callback (required but does nothing in tests).
        lv_display_set_flush_cb(disp, [](lv_display_t* disp, const lv_area_t* /*area*/, uint8_t* /*px_map*/) {
            // Mark the flush as complete immediately.
            // In real applications, this would copy pixels to the display.
            lv_display_flush_ready(disp);
        });
        
        return disp;
    }

protected:
    void SetUp() override {
        // Initialize LVGL.
        lv_init();
        
        // Create a minimal display for testing.
        display_ = createTestDisplay();
        
        // Create and load a screen.
        screen_ = lv_obj_create(NULL);
        lv_scr_load(screen_);
    }
    
    void TearDown() override {
        // Clean up in reverse order.
        if (screen_) {
            lv_obj_del(screen_);
            screen_ = nullptr;
        }
        
        if (display_) {
            lv_display_delete(display_);
            display_ = nullptr;
        }
        
        lv_deinit();
    }
    
    /**
     * Runs LVGL event loop for a specified duration.
     * This method advances LVGL's internal time and processes events/timers.
     * 
     * IMPORTANT: This method calls lv_tick_inc() to advance LVGL's internal
     * time, which is necessary for timers to function properly in tests.
     * 
     * @param duration_ms Total duration to run in milliseconds
     * @param step_ms Time to advance per iteration (default: 5ms)
     */
    void runLVGL(int duration_ms, int step_ms = 5) {
        for (int elapsed = 0; elapsed < duration_ms; elapsed += step_ms) {
            // Advance LVGL's internal time.
            lv_tick_inc(step_ms);
            
            // Process LVGL tasks and timers.
            lv_timer_handler();
            
            // Sleep to simulate real time passing.
            std::this_thread::sleep_for(std::chrono::milliseconds(step_ms));
        }
    }
    
    /**
     * Runs LVGL event loop until a condition is met or timeout occurs.
     * 
     * @param condition Function that returns true when the condition is met
     * @param timeout_ms Maximum time to wait in milliseconds
     * @param step_ms Time to advance per iteration (default: 5ms)
     * @return true if condition was met, false if timeout occurred
     */
    template<typename Predicate>
    bool runLVGLUntil(Predicate condition, int timeout_ms, int step_ms = 5) {
        for (int elapsed = 0; elapsed < timeout_ms; elapsed += step_ms) {
            if (condition()) {
                return true;
            }
            
            lv_tick_inc(step_ms);
            lv_timer_handler();
            std::this_thread::sleep_for(std::chrono::milliseconds(step_ms));
        }
        return false;
    }
    
    // Protected members accessible to derived test classes.
    lv_display_t* display_ = nullptr;
    lv_obj_t* screen_ = nullptr;
};