#include "UIEventTestBase.h"
#include "../ui/LVGLEventBuilder.h"
#include <gtest/gtest.h>

/**
 * @brief Tests for button UI event generation and routing.
 *
 * Verifies that button widgets correctly generate events and route them
 * through the event system to update world state.
 */
class ButtonEventTest : public UIEventTestBase {};

TEST_F(ButtonEventTest, DebugToggleButtonWorks) {
    bool initialDebugState = getWorld()->isDebugDrawEnabled();
    spdlog::info("[TEST] Initial debug state: {}", initialDebugState);

    // Create debug toggle button with event routing.
    lv_obj_t* btn = DirtSim::LVGLEventBuilder::button(getScreen(), getRouter())
                        .onDebugToggle()
                        .text("Debug: Off")
                        .buildOrLog();

    ASSERT_NE(btn, nullptr) << "Debug button should be created";

    // Simulate user clicking button.
    lv_obj_send_event(btn, LV_EVENT_CLICKED, nullptr);

    // Process queued events.
    processEvents();

    // Verify debug state toggled in world.
    bool newDebugState = getWorld()->isDebugDrawEnabled();
    spdlog::info("[TEST] New debug state: {}", newDebugState);

    EXPECT_NE(newDebugState, initialDebugState) << "Debug state should have toggled";
}

TEST_F(ButtonEventTest, PauseResumeButtonWorks) {
    // Check initial pause state.
    double initialTimescale = getWorld()->getTimescale();
    spdlog::info("[TEST] Initial timescale: {}", initialTimescale);

    // Create pause/resume toggle button.
    lv_obj_t* btn = DirtSim::LVGLEventBuilder::button(getScreen(), getRouter())
                        .onPauseResume()
                        .text("Pause")
                        .buildOrLog();

    ASSERT_NE(btn, nullptr) << "Pause button should be created";

    // Simulate user clicking pause button.
    lv_obj_send_event(btn, LV_EVENT_VALUE_CHANGED, nullptr);

    // Process queued events.
    processEvents();

    // Verify simulation was paused (timescale set to 0).
    double pausedTimescale = getWorld()->getTimescale();
    spdlog::info("[TEST] Paused timescale: {}", pausedTimescale);

    EXPECT_EQ(pausedTimescale, 0.0) << "Timescale should be 0.0 when paused";

    // Click again to resume.
    lv_obj_send_event(btn, LV_EVENT_VALUE_CHANGED, nullptr);
    processEvents();

    // Verify timescale restored.
    double resumedTimescale = getWorld()->getTimescale();
    spdlog::info("[TEST] Resumed timescale: {}", resumedTimescale);

    EXPECT_EQ(resumedTimescale, initialTimescale) << "Timescale should be restored after resume";
}

TEST_F(ButtonEventTest, GravitySliderWorks) {
    double initialGravity = getWorld()->getGravity();
    spdlog::info("[TEST] Initial gravity: {}", initialGravity);

    // Create gravity slider (-10x to +10x range).
    lv_obj_t* slider = DirtSim::LVGLEventBuilder::slider(getScreen(), getRouter())
                           .onGravityChange()
                           .range(-1000, 1000)
                           .value(-500)  // -5x Earth gravity = -49.05
                           .buildOrLog();

    ASSERT_NE(slider, nullptr) << "Gravity slider should be created";

    // Simulate user moving slider.
    lv_obj_send_event(slider, LV_EVENT_VALUE_CHANGED, nullptr);

    // Process queued events.
    processEvents();

    // Verify gravity changed to negative value.
    double newGravity = getWorld()->getGravity();
    spdlog::info("[TEST] New gravity: {}", newGravity);

    EXPECT_NE(newGravity, initialGravity) << "Gravity should have changed";
    EXPECT_NEAR(newGravity, -49.05, 0.1) << "Gravity should be -5x Earth gravity";
}
