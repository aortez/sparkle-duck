#include "../Event.h"
#include <gtest/gtest.h>
#include <chrono>
#include <thread>

/**
 * @brief Test suite for UIUpdateEvent structure and functionality.
 */
class UIUpdateEventTest : public ::testing::Test {
protected:
    UIUpdateEvent createDefaultEvent() {
        UIUpdateEvent event;
        event.fps = 60;
        event.stepCount = 1000;
        event.stats = SimulationStats{};
        event.physicsParams = PhysicsParams{};
        event.isPaused = false;
        event.debugEnabled = false;
        event.cohesionEnabled = true;
        event.adhesionEnabled = true;
        event.timeHistoryEnabled = false;
        event.selectedMaterial = MaterialType::DIRT;
        event.worldType = "World";
        event.timestamp = std::chrono::steady_clock::now();
        return event;
    }
};


TEST_F(UIUpdateEventTest, DefaultConstruction) {
    UIUpdateEvent event;
    
    // Verify dirty flags default to false.
    EXPECT_FALSE(event.dirty.fps);
    EXPECT_FALSE(event.dirty.stats);
    EXPECT_FALSE(event.dirty.physicsParams);
    EXPECT_FALSE(event.dirty.uiState);
    EXPECT_FALSE(event.dirty.worldState);
}

TEST_F(UIUpdateEventTest, EventName) {
    // Verify the event has the correct name.
    EXPECT_STREQ(UIUpdateEvent::name(), "UIUpdateEvent");
}

TEST_F(UIUpdateEventTest, SimulationStatsIntegration) {
    UIUpdateEvent event = createDefaultEvent();
    
    // Set some stats.
    event.stats.totalCells = 10000;
    event.stats.activeCells = 5000;
    event.stats.emptyCells = 5000;
    event.stats.totalMass = 1234.5;
    event.stats.avgVelocity = 0.25;
    event.stats.maxPressure = 10.0;
    
    EXPECT_EQ(event.stats.totalCells, 10000);
    EXPECT_EQ(event.stats.activeCells, 5000);
    EXPECT_DOUBLE_EQ(event.stats.totalMass, 1234.5);
}

TEST_F(UIUpdateEventTest, PhysicsParamsIntegration) {
    UIUpdateEvent event = createDefaultEvent();
    
    // Verify default physics params.
    EXPECT_DOUBLE_EQ(event.physicsParams.gravity, 9.81);
    EXPECT_DOUBLE_EQ(event.physicsParams.elasticity, 0.8);
    EXPECT_DOUBLE_EQ(event.physicsParams.timescale, 1.0);
    EXPECT_FALSE(event.debugEnabled);  // debugEnabled now in UIUpdateEvent directly.
    // gravityEnabled removed - check gravity != 0.0 instead.
    
    // Modify params.
    event.physicsParams.gravity = 19.62;
    // debugEnabled removed from physicsParams - now in event.debugEnabled.
    event.debugEnabled = true;

    EXPECT_DOUBLE_EQ(event.physicsParams.gravity, 19.62);
    EXPECT_TRUE(event.debugEnabled);
}

TEST_F(UIUpdateEventTest, DirtyFlagsUsage) {
    UIUpdateEvent event = createDefaultEvent();
    
    // Simulate marking different components as dirty.
    event.dirty.fps = true;
    event.dirty.uiState = true;
    
    EXPECT_TRUE(event.dirty.fps);
    EXPECT_FALSE(event.dirty.stats);
    EXPECT_FALSE(event.dirty.physicsParams);
    EXPECT_TRUE(event.dirty.uiState);
    EXPECT_FALSE(event.dirty.worldState);
}

TEST_F(UIUpdateEventTest, TimestampLatency) {
    UIUpdateEvent event = createDefaultEvent();
    
    // Simulate some processing delay.
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    
    auto now = std::chrono::steady_clock::now();
    auto latency = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - event.timestamp);
    
    // Latency should be at least 10ms.
    EXPECT_GE(latency.count(), 10);
}

TEST_F(UIUpdateEventTest, MaterialTypeIntegration) {
    UIUpdateEvent event = createDefaultEvent();
    
    // Test all material types.
    std::vector<MaterialType> materials = {
        MaterialType::AIR,
        MaterialType::DIRT,
        MaterialType::WATER,
        MaterialType::WOOD,
        MaterialType::SAND,
        MaterialType::METAL,
        MaterialType::LEAF,
        MaterialType::WALL
    };
    
    for (auto material : materials) {
        event.selectedMaterial = material;
        EXPECT_EQ(event.selectedMaterial, material);
    }
}

TEST_F(UIUpdateEventTest, WorldTypeString) {
    UIUpdateEvent event = createDefaultEvent();
    
    // Test different world type strings.
    event.worldType = "WorldA";
    EXPECT_EQ(event.worldType, "WorldA");
    
    event.worldType = "World";
    EXPECT_EQ(event.worldType, "World");
    
    event.worldType = "None";
    EXPECT_EQ(event.worldType, "None");
}

TEST_F(UIUpdateEventTest, ComprehensiveStateCapture) {
    UIUpdateEvent event;
    
    // Set all fields to non-default values.
    event.fps = 144;
    event.stepCount = 999999;
    
    event.stats.totalCells = 40000;
    event.stats.dirtCells = 10000;
    event.stats.waterCells = 5000;
    event.stats.totalMass = 15000.0;
    event.stats.avgPressure = 5.5;
    
    event.physicsParams.gravity = 4.9;
    event.physicsParams.elasticity = 0.95;
    event.physicsParams.timescale = 2.0;
    event.cohesionEnabled = false;
    event.adhesionEnabled = false;
    event.timeHistoryEnabled = true;

    event.isPaused = true;
    event.debugEnabled = true;
    event.cohesionEnabled = false;
    event.adhesionEnabled = false;
    event.timeHistoryEnabled = true;
    
    event.selectedMaterial = MaterialType::METAL;
    event.worldType = "CustomWorld";
    
    event.dirty.fps = true;
    event.dirty.stats = true;
    event.dirty.physicsParams = true;
    event.dirty.uiState = true;
    event.dirty.worldState = true;
    
    // Verify all values.
    EXPECT_EQ(event.fps, 144);
    EXPECT_EQ(event.stepCount, 999999);
    EXPECT_EQ(event.stats.totalCells, 40000);
    EXPECT_DOUBLE_EQ(event.physicsParams.gravity, 4.9);
    EXPECT_TRUE(event.isPaused);
    EXPECT_EQ(event.selectedMaterial, MaterialType::METAL);
    EXPECT_EQ(event.worldType, "CustomWorld");
    EXPECT_TRUE(event.dirty.fps);
    EXPECT_TRUE(event.dirty.stats);
    EXPECT_TRUE(event.dirty.physicsParams);
    EXPECT_TRUE(event.dirty.uiState);
    EXPECT_TRUE(event.dirty.worldState);
}

TEST_F(UIUpdateEventTest, CopySemantics) {
    UIUpdateEvent original = createDefaultEvent();
    original.fps = 120;
    original.worldType = "TestWorld";
    original.dirty.fps = true;
    
    // Copy construction.
    UIUpdateEvent copy(original);
    EXPECT_EQ(copy.fps, 120);
    EXPECT_EQ(copy.worldType, "TestWorld");
    EXPECT_TRUE(copy.dirty.fps);
    
    // Copy assignment.
    UIUpdateEvent assigned;
    assigned = original;
    EXPECT_EQ(assigned.fps, 120);
    EXPECT_EQ(assigned.worldType, "TestWorld");
    EXPECT_TRUE(assigned.dirty.fps);
}

TEST_F(UIUpdateEventTest, MoveSemantics) {
    UIUpdateEvent original = createDefaultEvent();
    original.fps = 240;
    original.worldType = "MovedWorld";
    original.stats.totalMass = 9999.9;
    
    // Move construction.
    UIUpdateEvent moved(std::move(original));
    EXPECT_EQ(moved.fps, 240);
    EXPECT_EQ(moved.worldType, "MovedWorld");
    EXPECT_DOUBLE_EQ(moved.stats.totalMass, 9999.9);
    
    // Move assignment.
    UIUpdateEvent assigned;
    UIUpdateEvent temp = createDefaultEvent();
    temp.fps = 360;
    assigned = std::move(temp);
    EXPECT_EQ(assigned.fps, 360);
}