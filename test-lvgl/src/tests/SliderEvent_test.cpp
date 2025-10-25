#include "UIEventTestBase.h"
#include "../ui/LVGLEventBuilder.h"
#include <gtest/gtest.h>

/**
 * @brief Tests for slider UI event generation and routing.
 *
 * Verifies that slider widgets correctly generate events and route them
 * through the event system to update world state.
 */
class SliderEventTest : public UIEventTestBase {};

TEST_F(SliderEventTest, TimescaleSliderGeneratesEvent) {
    double initialTimescale = getWorld()->getTimescale();
    spdlog::info("[TEST] Initial timescale: {}", initialTimescale);

    // Create timescale slider with event routing.
    lv_obj_t* slider = DirtSim::LVGLEventBuilder::slider(getScreen(), getRouter())
                           .onTimescaleChange()
                           .range(0, 100)
                           .value(75)  // 75 on log scale = 10^((75-50)/50) = 3.16x
                           .buildOrLog();

    ASSERT_NE(slider, nullptr) << "Slider should be created";

    // Simulate user moving slider.
    lv_obj_send_event(slider, LV_EVENT_VALUE_CHANGED, nullptr);

    // Process queued events.
    processEvents();

    // Verify timescale changed in world.
    double newTimescale = getWorld()->getTimescale();
    spdlog::info("[TEST] New timescale: {}", newTimescale);

    EXPECT_NE(newTimescale, initialTimescale) << "Timescale should have changed";
    EXPECT_NEAR(newTimescale, 3.16, 0.1) << "Timescale should be ~3.16x";
}

TEST_F(SliderEventTest, ElasticitySliderGeneratesEvent) {
    double initialElasticity = getWorld()->getElasticityFactor();
    spdlog::info("[TEST] Initial elasticity: {}", initialElasticity);

    // Create elasticity slider with event routing.
    lv_obj_t* slider = DirtSim::LVGLEventBuilder::slider(getScreen(), getRouter())
                           .onElasticityChange()
                           .range(0, 200)
                           .value(150)  // 150/100 = 1.5
                           .buildOrLog();

    ASSERT_NE(slider, nullptr) << "Slider should be created";

    // Simulate user moving slider.
    lv_obj_send_event(slider, LV_EVENT_VALUE_CHANGED, nullptr);

    // Process queued events.
    processEvents();

    // Verify elasticity changed in world.
    double newElasticity = getWorld()->getElasticityFactor();
    spdlog::info("[TEST] New elasticity: {}", newElasticity);

    EXPECT_NE(newElasticity, initialElasticity) << "Elasticity should have changed";
    EXPECT_NEAR(newElasticity, 1.5, 0.01) << "Elasticity should be 1.5";
}
