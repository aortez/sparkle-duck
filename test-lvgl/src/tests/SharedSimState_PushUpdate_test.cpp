#include "../SharedSimState.h"
#include <gtest/gtest.h>
#include <thread>

/**
 * @brief Test suite for SharedSimState push-based update system.
 */
class SharedSimStatePushUpdateTest : public ::testing::Test {
protected:
    SharedSimState sharedState;
    
    // Helper to create a test UIUpdateEvent.
    UIUpdateEvent createTestEvent(uint32_t fps = 60) {
        UIUpdateEvent event;
        event.fps = fps;
        event.stepCount = 1000;
        event.isPaused = false;
        event.selectedMaterial = MaterialType::DIRT;
        event.worldType = "WorldB";
        event.timestamp = std::chrono::steady_clock::now();
        return event;
    }
};

TEST_F(SharedSimStatePushUpdateTest, FeatureFlagDefaults) {
    // Push updates should be disabled by default.
    EXPECT_FALSE(sharedState.isPushUpdatesEnabled());
}

TEST_F(SharedSimStatePushUpdateTest, FeatureFlagToggle) {
    // Enable push updates.
    sharedState.enablePushUpdates(true);
    EXPECT_TRUE(sharedState.isPushUpdatesEnabled());
    
    // Disable push updates.
    sharedState.enablePushUpdates(false);
    EXPECT_FALSE(sharedState.isPushUpdatesEnabled());
}

TEST_F(SharedSimStatePushUpdateTest, PushWhenDisabled) {
    // Ensure push updates are disabled.
    sharedState.enablePushUpdates(false);
    
    // Push an update.
    sharedState.pushUIUpdate(createTestEvent());
    
    // Should not be queued.
    EXPECT_FALSE(sharedState.hasUIUpdatePending());
    auto popped = sharedState.popUIUpdate();
    EXPECT_FALSE(popped.has_value());
}

TEST_F(SharedSimStatePushUpdateTest, PushWhenEnabled) {
    // Enable push updates.
    sharedState.enablePushUpdates(true);
    
    // Push an update.
    sharedState.pushUIUpdate(createTestEvent(144));
    
    // Should be queued.
    EXPECT_TRUE(sharedState.hasUIUpdatePending());
    
    // Pop and verify.
    auto popped = sharedState.popUIUpdate();
    ASSERT_TRUE(popped.has_value());
    EXPECT_EQ(popped->fps, 144);
    
    // Queue should be empty.
    EXPECT_FALSE(sharedState.hasUIUpdatePending());
}

TEST_F(SharedSimStatePushUpdateTest, MetricsAccess) {
    sharedState.enablePushUpdates(true);
    
    // Push several updates.
    for (int i = 0; i < 5; ++i) {
        sharedState.pushUIUpdate(createTestEvent(i * 10));
    }
    
    // Pop one.
    sharedState.popUIUpdate();
    
    // Check metrics.
    auto metrics = sharedState.getUIUpdateMetrics();
    EXPECT_EQ(metrics.pushCount, 5);
    EXPECT_EQ(metrics.popCount, 1);
    EXPECT_EQ(metrics.dropCount, 4); // 4 updates were overwritten.
}

TEST_F(SharedSimStatePushUpdateTest, ThreadSafetyWithOtherState) {
    // Test that push updates don't interfere with other shared state operations.
    sharedState.enablePushUpdates(true);
    
    std::thread pushThread([this]() {
        for (int i = 0; i < 1000; ++i) {
            sharedState.pushUIUpdate(createTestEvent(i));
        }
    });
    
    std::thread popThread([this]() {
        for (int i = 0; i < 1000; ++i) {
            sharedState.popUIUpdate();
        }
    });
    
    std::thread stateThread([this]() {
        for (int i = 0; i < 1000; ++i) {
            // Modify other shared state.
            sharedState.setCurrentStep(i);
            sharedState.setCurrentFPS(60.0f + i % 30);
            sharedState.setIsPaused(i % 2 == 0);
            sharedState.setSelectedMaterial(
                static_cast<MaterialType>(i % static_cast<int>(MaterialType::WALL)));
            
            // Read state.
            auto step = sharedState.getCurrentStep();
            auto fps = sharedState.getCurrentFPS();
            auto paused = sharedState.getIsPaused();
            auto material = sharedState.getSelectedMaterial();
            
            // Basic sanity checks.
            EXPECT_GE(step, 0);
            EXPECT_GE(fps, 0.0f);
            (void)paused; // Suppress unused warning.
            (void)material;
        }
    });
    
    pushThread.join();
    popThread.join();
    stateThread.join();
    
    // Verify state is still consistent.
    EXPECT_GE(sharedState.getCurrentStep(), 0);
    EXPECT_GE(sharedState.getCurrentFPS(), 0.0f);
}

TEST_F(SharedSimStatePushUpdateTest, CompleteUIUpdateFlow) {
    // Enable push updates.
    sharedState.enablePushUpdates(true);
    
    // Set up some state.
    sharedState.setCurrentStep(12345);
    sharedState.setCurrentFPS(59.5f);
    sharedState.setIsPaused(true);
    sharedState.setSelectedMaterial(MaterialType::WATER);
    
    // Create and push a UI update.
    UIUpdateEvent update;
    update.fps = static_cast<uint32_t>(sharedState.getCurrentFPS());
    update.stepCount = sharedState.getCurrentStep();
    update.isPaused = sharedState.getIsPaused();
    update.selectedMaterial = sharedState.getSelectedMaterial();
    
    // Set physics params directly (no longer cached in SharedSimState).
    update.physicsParams.gravity = 9.81;
    update.physicsParams.elasticity = 0.8;
    update.physicsParams.timescale = 1.0;
    
    update.worldType = "WorldA";
    update.timestamp = std::chrono::steady_clock::now();
    
    sharedState.pushUIUpdate(std::move(update));
    
    // Pop and verify.
    auto popped = sharedState.popUIUpdate();
    ASSERT_TRUE(popped.has_value());
    EXPECT_EQ(popped->fps, 59);
    EXPECT_EQ(popped->stepCount, 12345);
    EXPECT_TRUE(popped->isPaused);
    EXPECT_EQ(popped->selectedMaterial, MaterialType::WATER);
    EXPECT_EQ(popped->physicsParams.gravity, 9.81);
    EXPECT_TRUE(popped->debugEnabled);
    EXPECT_TRUE(popped->forceEnabled);
    EXPECT_EQ(popped->worldType, "WorldA");
}