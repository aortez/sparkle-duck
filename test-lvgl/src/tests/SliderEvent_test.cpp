#include "UIEventTestBase.h"
#include "../ui/LVGLEventBuilder.h"
#include "../Cell.h"
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

TEST_F(SliderEventTest, CohesionForceStrengthSliderWorks) {
    double initialStrength = getWorld()->getCohesionComForceStrength();
    spdlog::info("[TEST] Initial cohesion force strength: {}", initialStrength);

    lv_obj_t* slider = DirtSim::LVGLEventBuilder::slider(getScreen(), getRouter())
                           .onCohesionForceStrengthChange()
                           .range(0, 30000)
                           .value(20000)  // 200.0
                           .buildOrLog();

    ASSERT_NE(slider, nullptr) << "Slider should be created";

    lv_obj_send_event(slider, LV_EVENT_VALUE_CHANGED, nullptr);
    processEvents();

    double newStrength = getWorld()->getCohesionComForceStrength();
    spdlog::info("[TEST] New cohesion force strength: {}", newStrength);

    EXPECT_NE(newStrength, initialStrength) << "Cohesion strength should have changed";
    EXPECT_NEAR(newStrength, 200.0, 0.1) << "Cohesion strength should be 200.0";
}

TEST_F(SliderEventTest, ViscosityStrengthSliderWorks) {
    double initialStrength = getWorld()->getViscosityStrength();
    spdlog::info("[TEST] Initial viscosity strength: {}", initialStrength);

    lv_obj_t* slider = DirtSim::LVGLEventBuilder::slider(getScreen(), getRouter())
                           .onViscosityStrengthChange()
                           .range(0, 200)
                           .value(150)  // 1.5
                           .buildOrLog();

    ASSERT_NE(slider, nullptr) << "Slider should be created";

    lv_obj_send_event(slider, LV_EVENT_VALUE_CHANGED, nullptr);
    processEvents();

    double newStrength = getWorld()->getViscosityStrength();
    spdlog::info("[TEST] New viscosity strength: {}", newStrength);

    EXPECT_NE(newStrength, initialStrength) << "Viscosity strength should have changed";
    EXPECT_NEAR(newStrength, 1.5, 0.01) << "Viscosity strength should be 1.5";
}

TEST_F(SliderEventTest, AdhesionStrengthSliderWorks) {
    double initialStrength = getWorld()->getAdhesionStrength();
    spdlog::info("[TEST] Initial adhesion strength: {}", initialStrength);

    lv_obj_t* slider = DirtSim::LVGLEventBuilder::slider(getScreen(), getRouter())
                           .onAdhesionStrengthChange()
                           .range(0, 1000)
                           .value(800)  // 8.0
                           .buildOrLog();

    ASSERT_NE(slider, nullptr) << "Slider should be created";

    lv_obj_send_event(slider, LV_EVENT_VALUE_CHANGED, nullptr);
    processEvents();

    double newStrength = getWorld()->getAdhesionStrength();
    spdlog::info("[TEST] New adhesion strength: {}", newStrength);

    EXPECT_NE(newStrength, initialStrength) << "Adhesion strength should have changed";
    EXPECT_NEAR(newStrength, 8.0, 0.01) << "Adhesion strength should be 8.0";
}

TEST_F(SliderEventTest, RainRateSliderWorks) {
    double initialRate = getWorld()->getRainRate();
    spdlog::info("[TEST] Initial rain rate: {}", initialRate);

    lv_obj_t* slider = DirtSim::LVGLEventBuilder::slider(getScreen(), getRouter())
                           .onRainRateChange()
                           .range(0, 100)
                           .value(50)
                           .buildOrLog();

    ASSERT_NE(slider, nullptr) << "Slider should be created";

    lv_obj_send_event(slider, LV_EVENT_VALUE_CHANGED, nullptr);
    processEvents();

    double newRate = getWorld()->getRainRate();
    spdlog::info("[TEST] New rain rate: {}", newRate);

    EXPECT_NE(newRate, initialRate) << "Rain rate should have changed";
    EXPECT_NEAR(newRate, 50.0, 0.01) << "Rain rate should be 50.0";
}

TEST_F(SliderEventTest, CellSizeSliderWorks) {
    uint32_t initialSize = Cell::getSize();
    spdlog::info("[TEST] Initial cell size: {}", initialSize);

    lv_obj_t* slider = DirtSim::LVGLEventBuilder::slider(getScreen(), getRouter())
                           .onCellSizeChange()
                           .range(10, 100)
                           .value(75)
                           .buildOrLog();

    ASSERT_NE(slider, nullptr) << "Slider should be created";

    lv_obj_send_event(slider, LV_EVENT_VALUE_CHANGED, nullptr);
    processEvents();

    uint32_t newSize = Cell::getSize();
    spdlog::info("[TEST] Cell size after slider: {}", newSize);

    EXPECT_NE(newSize, initialSize) << "Cell size should have changed";
    EXPECT_EQ(newSize, 75) << "Cell size should be 75";
}
