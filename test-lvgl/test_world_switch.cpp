#include <gtest/gtest.h>
#include "SimulationManager.h"
#include "WorldFactory.h"
#include "lvgl/lvgl.h"

class WorldSwitchTest : public ::testing::Test {
protected:
    void SetUp() override {
        lv_init();
        // Create a mock screen
        screen = lv_obj_create(NULL);
    }
    
    void TearDown() override {
        if (screen) {
            lv_obj_del(screen);
        }
        lv_deinit();
    }
    
    lv_obj_t* screen;
};

TEST_F(WorldSwitchTest, WorldSwitchNoCrash) {
    // Create manager with WorldB (default)
    auto manager = std::make_unique<SimulationManager>(WorldType::RulesB, 10, 10, screen);
    manager->initialize();
    
    // Verify initial world type
    EXPECT_EQ(manager->getWorld()->getWorldType(), WorldType::RulesB);
    
    // Switch to WorldA - this should not crash
    EXPECT_NO_THROW(manager->switchWorldType(WorldType::RulesA));
    EXPECT_EQ(manager->getWorld()->getWorldType(), WorldType::RulesA);
    
    // Add some material and advance time - this would crash with stale references
    EXPECT_NO_THROW(manager->getWorld()->addDirtAtPixel(50, 50));
    EXPECT_NO_THROW(manager->getWorld()->advanceTime(0.016));
    EXPECT_NO_THROW(manager->getWorld()->draw());
    
    // Switch back to WorldB
    EXPECT_NO_THROW(manager->switchWorldType(WorldType::RulesB));
    EXPECT_EQ(manager->getWorld()->getWorldType(), WorldType::RulesB);
    
    // Test interactions work after switch back
    EXPECT_NO_THROW(manager->getWorld()->addWaterAtPixel(100, 100));
    EXPECT_NO_THROW(manager->getWorld()->advanceTime(0.016));
    EXPECT_NO_THROW(manager->getWorld()->draw());
}