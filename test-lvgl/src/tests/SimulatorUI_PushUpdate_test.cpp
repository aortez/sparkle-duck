#include <gtest/gtest.h>
#include "../Event.h"
#include "../SimulatorUI.h"
#include "../SimulationStats.h"
#include "../MaterialType.h"
#include <chrono>
#include <thread>

/**
 * @brief Test fixture for SimulatorUI applyUpdate functionality
 * 
 * Tests the push-based UI update system's integration with SimulatorUI,
 * verifying that UI elements are correctly updated based on UIUpdateEvent data
 * and that dirty flags are used efficiently.
 */
class SimulatorUIPushUpdateTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a minimal SimulatorUI without LVGL dependencies.
        // In real tests, we'd mock LVGL or use a test harness.
        ui_ = std::make_unique<SimulatorUI>(nullptr, nullptr);
    }

    UIUpdateEvent createTestUpdate() {
        UIUpdateEvent event;
        event.sequenceNum = next_seq_++;
        event.fps = 60;
        event.stepCount = 1000;
        
        // Initialize stats.
        event.stats.totalMass = 123.45;
        event.stats.activeCells = 500;
        event.stats.totalCells = 1000;
        
        // Initialize physics params.
        event.physicsParams.gravity = 9.81;
        event.physicsParams.elasticity = 0.8;
        event.physicsParams.timescale = 1.0;
        // debugEnabled removed from physicsParams - now in event.debugEnabled.

        // Initialize UI state.
        event.isPaused = false;
        event.debugEnabled = false;
        event.cohesionEnabled = true;
        event.adhesionEnabled = true;
        event.timeHistoryEnabled = false;
        
        // Initialize world state.
        event.selectedMaterial = MaterialType::DIRT;
        event.worldType = "World";
        
        event.timestamp = std::chrono::steady_clock::now();
        
        // Clear all dirty flags by default.
        event.dirty = {};
        
        return event;
    }

    std::unique_ptr<SimulatorUI> ui_;
    uint64_t next_seq_ = 1;
};

/**
 * Test that applyUpdate correctly updates FPS when dirty flag is set
 */
TEST_F(SimulatorUIPushUpdateTest, UpdatesFPSWhenDirty) {
    auto update = createTestUpdate();
    update.fps = 120;
    update.dirty.fps = true;
    
    // Apply the update.
    ui_->applyUpdate(update);
    
    // In a real test, we'd verify the FPS label was updated.
    // For now, this test ensures the method exists and handles the update.
}

/**
 * Test that applyUpdate correctly updates mass label when stats are dirty
 */
TEST_F(SimulatorUIPushUpdateTest, UpdatesMassWhenStatsDirty) {
    auto update = createTestUpdate();
    update.stats.totalMass = 999.99;
    update.dirty.stats = true;
    
    // Apply the update.
    ui_->applyUpdate(update);
    
    // In a real test, we'd verify the mass label was updated to 999.99.
}

/**
 * Test that applyUpdate skips updates when dirty flags are false
 */
TEST_F(SimulatorUIPushUpdateTest, SkipsUpdatesWhenNotDirty) {
    auto update = createTestUpdate();
    
    // Change values but don't set dirty flags.
    update.fps = 30;
    update.stats.totalMass = 777.77;
    update.isPaused = true;
    
    // All dirty flags are false.
    update.dirty = {};
    
    // Apply the update - should skip all updates.
    ui_->applyUpdate(update);
    
    // In a real test, we'd verify no UI elements were updated.
}

/**
 * Test that applyUpdate correctly updates pause state
 */
TEST_F(SimulatorUIPushUpdateTest, UpdatesPauseStateWhenDirty) {
    auto update = createTestUpdate();
    update.isPaused = true;
    update.dirty.uiState = true;
    
    // Apply the update.
    ui_->applyUpdate(update);
    
    // In a real test, we'd verify the pause label shows "Paused"
}

/**
 * Test that applyUpdate correctly updates debug state
 */
TEST_F(SimulatorUIPushUpdateTest, UpdatesDebugStateWhenDirty) {
    auto update = createTestUpdate();
    update.debugEnabled = true;
    update.dirty.uiState = true;
    
    // Apply the update.
    ui_->applyUpdate(update);

    // In a real test, we'd verify world->isDebugDrawEnabled() is set to true.
    // and the debug button shows "Debug: On".
}

/**
 * Test that applyUpdate correctly updates world type
 */
TEST_F(SimulatorUIPushUpdateTest, UpdatesWorldTypeWhenDirty) {
    auto update = createTestUpdate();
    update.worldType = "WorldA";
    update.dirty.worldState = true;
    
    // Apply the update.
    ui_->applyUpdate(update);
    
    // In a real test, we'd verify the world type button matrix is updated.
}

/**
 * Test that applyUpdate handles multiple dirty flags correctly
 */
TEST_F(SimulatorUIPushUpdateTest, HandlesMultipleDirtyFlags) {
    auto update = createTestUpdate();
    
    // Set multiple values and dirty flags.
    update.fps = 144;
    update.dirty.fps = true;
    
    update.stats.totalMass = 555.55;
    update.dirty.stats = true;
    
    update.isPaused = true;
    update.debugEnabled = true;
    update.dirty.uiState = true;
    
    // Apply the update.
    ui_->applyUpdate(update);
    
    // All relevant UI elements should be updated.
}

/**
 * Test rapid updates to verify efficiency
 */
TEST_F(SimulatorUIPushUpdateTest, HandlesRapidUpdatesEfficiently) {
    // Simulate 60fps updates for 1 second.
    auto start = std::chrono::steady_clock::now();
    
    for (int i = 0; i < 60; i++) {
        auto update = createTestUpdate();
        update.fps = 60 + i;  // Slightly varying FPS.
        update.dirty.fps = true;
        
        // Only update stats every 10 frames.
        if (i % 10 == 0) {
            update.stats.totalMass = 100.0 + i;
            update.dirty.stats = true;
        }
        
        ui_->applyUpdate(update);
        
        // Sleep to simulate 60fps timing.
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }
    
    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    // Should complete in approximately 1 second (60 * 16ms = 960ms).
    EXPECT_GE(duration.count(), 900);  // At least 900ms.
    EXPECT_LE(duration.count(), 1100); // At most 1100ms.
}

/**
 * Test that physics parameters are handled correctly
 * (Currently these don't update UI directly, but the structure is in place)
 */
TEST_F(SimulatorUIPushUpdateTest, HandlesPhysicsParamsWhenDirty) {
    auto update = createTestUpdate();
    update.physicsParams.gravity = 19.62;  // Double gravity.
    update.physicsParams.elasticity = 0.5;
    update.physicsParams.timescale = 2.0;
    update.dirty.physicsParams = true;
    
    // Apply the update.
    ui_->applyUpdate(update);
    
    // In a real implementation, this might update slider positions.
    // or other UI elements that display physics parameters.
}

/**
 * Test edge case: empty world type string
 */
TEST_F(SimulatorUIPushUpdateTest, HandlesEmptyWorldType) {
    auto update = createTestUpdate();
    update.worldType = "";
    update.dirty.worldState = true;
    
    // Apply the update - should handle gracefully.
    ui_->applyUpdate(update);
}

/**
 * Test edge case: invalid world type string
 */
TEST_F(SimulatorUIPushUpdateTest, HandlesInvalidWorldType) {
    auto update = createTestUpdate();
    update.worldType = "WorldC";  // Invalid type.
    update.dirty.worldState = true;
    
    // Apply the update - should handle gracefully.
    ui_->applyUpdate(update);
}