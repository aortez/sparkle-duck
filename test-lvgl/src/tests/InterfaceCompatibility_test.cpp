#include "visual_test_runner.h"
#include "../WorldInterface.h"
#include "../World.h"
#include "../WorldB.h"
#include "../WorldFactory.h"
#include "../MaterialType.h"
#include <spdlog/spdlog.h>
#include <gtest/gtest.h>

class InterfaceCompatibilityTest : public VisualTestBase, 
                                   public ::testing::WithParamInterface<WorldType> {
protected:
    void SetUp() override {
        VisualTestBase::SetUp();
        
        // Create world based on parameter
        WorldType type = GetParam();
        world = createTestWorld(type, 5, 5);  // 5x5 grid for testing
        
        if (!world) {
            FAIL() << "Failed to create world of type " << static_cast<int>(type);
        }
        
        // Common setup for both world types
        world->setAddParticlesEnabled(false);
        world->setTimescale(1.0);
        
        spdlog::info("Created {} world for interface compatibility testing", 
                     type == WorldType::RulesA ? "World (RulesA)" : "WorldB (RulesB)");
    }
    
    void TearDown() override {
        world.reset();
        VisualTestBase::TearDown();
    }
    
    std::unique_ptr<WorldInterface> createTestWorld(WorldType type, uint32_t width, uint32_t height) {
        auto world = ::createWorld(type, width, height, nullptr);
        
        // Common test setup
        if (type == WorldType::RulesB) {
            // Disable walls for consistent testing  
            world->setWallsEnabled(false);
            world->reset();
        }
        
        return world;
    }
    
    WorldType getWorldType() const {
        return GetParam();
    }
    
    std::string getWorldTypeName() const {
        return getWorldType() == WorldType::RulesA ? "World(RulesA)" : "WorldB(RulesB)";
    }
    
    // Test data members
    std::unique_ptr<WorldInterface> world;
};

// =================================================================
// CORE INTERFACE METHOD TESTS
// =================================================================

TEST_P(InterfaceCompatibilityTest, GridAccessMethods) {
    spdlog::info("Testing grid access methods for {}", getWorldTypeName());
    
    // Test basic grid properties
    EXPECT_EQ(world->getWidth(), 5);
    EXPECT_EQ(world->getHeight(), 5);
    // Note: World starts with timestep 1, WorldB starts with 0
    EXPECT_GE(world->getTimestep(), 0);  // Should be >= 0
    
    // Draw area should be null (no UI in tests)
    EXPECT_EQ(world->getDrawArea(), nullptr);
    
    spdlog::info("Grid access methods validated for {}", getWorldTypeName());
}

TEST_P(InterfaceCompatibilityTest, SimulationControl) {
    spdlog::info("Testing simulation control for {}", getWorldTypeName());
    
    // Test timescale
    world->setTimescale(2.0);
    // Note: No getter for timescale in interface, but this tests the setter works
    
    // Test advance time (should work without crashing)
    uint32_t initialTimestep = world->getTimestep();
    world->advanceTime(0.016);
    EXPECT_GT(world->getTimestep(), initialTimestep);
    
    // Test reset
    uint32_t timestepBeforeReset = world->getTimestep();
    world->reset();
    // After reset, timestep should be reset (may vary by implementation)
    uint32_t timestepAfterReset = world->getTimestep();
    spdlog::info("Timestep before reset: {}, after reset: {}", timestepBeforeReset, timestepAfterReset);
    
    spdlog::info("Simulation control validated for {}", getWorldTypeName());
}

TEST_P(InterfaceCompatibilityTest, MaterialAddition) {
    spdlog::info("Testing material addition for {}", getWorldTypeName());
    
    double initialMass = world->getTotalMass();
    double initialRemovedMass = world->getRemovedMass();
    
    // Add dirt at center (convert cell coordinates to pixels)
    world->addDirtAtPixel(200, 200);  // Cell (2,2) - center of 5x5 grid
    
    double massAfterDirt = world->getTotalMass();
    EXPECT_GT(massAfterDirt, initialMass);
    
    // Add water at different location  
    world->addWaterAtPixel(100, 100);  // Cell (1,1) - different from dirt location
    
    double massAfterWater = world->getTotalMass();
    EXPECT_GT(massAfterWater, massAfterDirt);
    
    // Removed mass should still be initial value
    EXPECT_EQ(world->getRemovedMass(), initialRemovedMass);
    
    spdlog::info("Material addition validated for {} - initial: {}, after dirt: {}, after water: {}", 
                 getWorldTypeName(), initialMass, massAfterDirt, massAfterWater);
}

TEST_P(InterfaceCompatibilityTest, PhysicsParameters) {
    spdlog::info("Testing physics parameters for {}", getWorldTypeName());
    
    // Test gravity setting (should not crash)
    world->setGravity(9.81);
    world->setGravity(0.0);    // Zero gravity
    world->setGravity(19.62);  // Double gravity
    
    // Test elasticity setting
    world->setElasticityFactor(0.5);
    world->setElasticityFactor(1.0);
    world->setElasticityFactor(0.0);
    
    // Test pressure scale
    world->setPressureScale(1.0);
    world->setPressureScale(2.0);
    world->setPressureScale(0.5);
    
    // Test dirt fragmentation (may be no-op for WorldB)
    world->setDirtFragmentationFactor(0.1);
    world->setDirtFragmentationFactor(0.0);
    
    spdlog::info("Physics parameters validated for {}", getWorldTypeName());
}

TEST_P(InterfaceCompatibilityTest, WaterPhysicsParameters) {
    spdlog::info("Testing water physics parameters for {}", getWorldTypeName());
    
    // Test water pressure threshold
    double defaultThreshold = world->getWaterPressureThreshold();
    EXPECT_GE(defaultThreshold, 0.0);
    
    world->setWaterPressureThreshold(0.001);
    EXPECT_NEAR(world->getWaterPressureThreshold(), 0.001, 0.0001);
    
    world->setWaterPressureThreshold(0.01);
    EXPECT_NEAR(world->getWaterPressureThreshold(), 0.01, 0.001);
    
    spdlog::info("Water physics parameters validated for {} - default threshold: {}", 
                 getWorldTypeName(), defaultThreshold);
}

TEST_P(InterfaceCompatibilityTest, PressureSystemSelection) {
    spdlog::info("Testing pressure system selection for {}", getWorldTypeName());
    
    // Test all pressure system types
    world->setPressureSystem(WorldInterface::PressureSystem::Original);
    EXPECT_EQ(world->getPressureSystem(), WorldInterface::PressureSystem::Original);
    
    world->setPressureSystem(WorldInterface::PressureSystem::TopDown);
    EXPECT_EQ(world->getPressureSystem(), WorldInterface::PressureSystem::TopDown);
    
    world->setPressureSystem(WorldInterface::PressureSystem::IterativeSettling);
    EXPECT_EQ(world->getPressureSystem(), WorldInterface::PressureSystem::IterativeSettling);
    
    // Reset to original
    world->setPressureSystem(WorldInterface::PressureSystem::Original);
    EXPECT_EQ(world->getPressureSystem(), WorldInterface::PressureSystem::Original);
    
    spdlog::info("Pressure system selection validated for {}", getWorldTypeName());
}

// =================================================================
// DRAG INTERACTION TESTS
// =================================================================

TEST_P(InterfaceCompatibilityTest, DragInteraction) {
    spdlog::info("Testing drag interaction for {}", getWorldTypeName());
    
    // Add some material first
    world->addDirtAtPixel(200, 200);  // Cell (2,2)
    double massBeforeDrag = world->getTotalMass();
    
    // Test drag sequence (behavior may vary between implementations)
    world->startDragging(2, 2);
    world->updateDrag(3, 3);
    world->updateDrag(1, 1);  // Stay within grid
    world->endDragging(1, 1);
    
    // Mass may not be conserved during drag (implementation-specific)
    double massAfterDrag = world->getTotalMass();
    
    // Test restore functionality
    world->restoreLastDragCell();
    double massAfterRestore = world->getTotalMass();
    EXPECT_GE(massAfterRestore, 0.0);
    
    // Just verify the methods don't crash
    
    spdlog::info("Drag interaction validated for {} - before: {}, after: {}, restored: {}", 
                 getWorldTypeName(), massBeforeDrag, massAfterDrag, massAfterRestore);
}

// =================================================================
// TIME REVERSAL TESTS
// =================================================================

TEST_P(InterfaceCompatibilityTest, TimeReversalFunctionality) {
    spdlog::info("Testing time reversal functionality for {}", getWorldTypeName());
    
    // Test time reversal enable/disable
    world->enableTimeReversal(true);
    bool isEnabled = world->isTimeReversalEnabled();
    
    if (getWorldType() == WorldType::RulesA) {
        // World should support time reversal
        EXPECT_TRUE(isEnabled);
    } else {
        // WorldB may not support time reversal (simplified implementation)
        // Just verify the method doesn't crash
        spdlog::info("WorldB time reversal enabled: {}", isEnabled);
    }
    
    world->enableTimeReversal(false);
    EXPECT_FALSE(world->isTimeReversalEnabled());
    
    // Test time reversal operations (may be no-ops for WorldB)
    world->saveWorldState();
    bool canGoBack = world->canGoBackward();
    bool canGoForward = world->canGoForward();
    size_t historySize = world->getHistorySize();
    
    if (canGoBack) {
        world->goBackward();
    }
    if (canGoForward) {
        world->goForward();
    }
    
    world->clearHistory();
    EXPECT_EQ(world->getHistorySize(), 0);
    
    spdlog::info("Time reversal validated for {} - canBack: {}, canForward: {}, historySize: {}", 
                 getWorldTypeName(), canGoBack, canGoForward, historySize);
}

// =================================================================
// WORLD SETUP TESTS
// =================================================================

TEST_P(InterfaceCompatibilityTest, WorldSetupControls) {
    spdlog::info("Testing world setup controls for {}", getWorldTypeName());
    
    // Test throw controls
    world->setLeftThrowEnabled(true);
    EXPECT_TRUE(world->isLeftThrowEnabled());
    world->setLeftThrowEnabled(false);
    EXPECT_FALSE(world->isLeftThrowEnabled());
    
    world->setRightThrowEnabled(true);
    EXPECT_TRUE(world->isRightThrowEnabled());
    world->setRightThrowEnabled(false);
    EXPECT_FALSE(world->isRightThrowEnabled());
    
    // Test quadrant control
    world->setLowerRightQuadrantEnabled(true);
    EXPECT_TRUE(world->isLowerRightQuadrantEnabled());
    world->setLowerRightQuadrantEnabled(false);
    EXPECT_FALSE(world->isLowerRightQuadrantEnabled());
    
    // Test walls control
    world->setWallsEnabled(true);
    EXPECT_TRUE(world->areWallsEnabled());
    world->setWallsEnabled(false);
    EXPECT_FALSE(world->areWallsEnabled());
    
    // Test rain rate
    world->setRainRate(0.0);
    EXPECT_NEAR(world->getRainRate(), 0.0, 0.001);
    world->setRainRate(1.5);
    EXPECT_NEAR(world->getRainRate(), 1.5, 0.001);
    
    spdlog::info("World setup controls validated for {}", getWorldTypeName());
}

TEST_P(InterfaceCompatibilityTest, ParticleAndCursorControls) {
    spdlog::info("Testing particle and cursor controls for {}", getWorldTypeName());
    
    // Test particle addition control
    world->setAddParticlesEnabled(true);
    world->setAddParticlesEnabled(false);
    
    // Test cursor force controls
    world->setCursorForceEnabled(true);
    world->updateCursorForce(100, 100, true);
    world->updateCursorForce(150, 150, false);
    world->clearCursorForce();
    world->setCursorForceEnabled(false);
    
    spdlog::info("Particle and cursor controls validated for {}", getWorldTypeName());
}

// =================================================================
// GRID RESIZING TESTS
// =================================================================

TEST_P(InterfaceCompatibilityTest, GridResizing) {
    spdlog::info("Testing grid resizing for {}", getWorldTypeName());
    
    // Add some material before resize
    world->addDirtAtPixel(100, 100);
    double massBeforeResize = world->getTotalMass();
    
    // Test resize to larger grid
    world->resizeGrid(7, 7);
    EXPECT_EQ(world->getWidth(), 7);
    EXPECT_EQ(world->getHeight(), 7);
    
    // Test resize to smaller grid
    world->resizeGrid(3, 3);
    EXPECT_EQ(world->getWidth(), 3);
    EXPECT_EQ(world->getHeight(), 3);
    
    // Resize back to original
    world->resizeGrid(5, 5);
    EXPECT_EQ(world->getWidth(), 5);
    EXPECT_EQ(world->getHeight(), 5);
    
    spdlog::info("Grid resizing validated for {} - mass before resize: {}", 
                 getWorldTypeName(), massBeforeResize);
}

// =================================================================
// PERFORMANCE AND DEBUGGING TESTS
// =================================================================

TEST_P(InterfaceCompatibilityTest, PerformanceAndDebugging) {
    spdlog::info("Testing performance and debugging for {}", getWorldTypeName());
    
    // Test timer stats (should not crash)
    world->dumpTimerStats();
    
    // Test user input marking
    world->markUserInput();
    
    spdlog::info("Performance and debugging validated for {}", getWorldTypeName());
}

// =================================================================
// MASS CONSERVATION TESTS
// =================================================================

TEST_P(InterfaceCompatibilityTest, MassConservation) {
    spdlog::info("Testing mass conservation for {}", getWorldTypeName());
    
    double initialMass = world->getTotalMass();
    double initialRemovedMass __attribute__((unused)) = world->getRemovedMass();
    
    // Add materials
    world->addDirtAtPixel(100, 100);  // Cell (1,1)
    world->addWaterAtPixel(200, 200);  // Cell (2,2)
    world->addDirtAtPixel(300, 300);  // Cell (3,3)
    
    double massAfterAddition = world->getTotalMass();
    EXPECT_GT(massAfterAddition, initialMass);
    
    // Run physics for a few steps
    for (int i = 0; i < 10; ++i) {
        world->advanceTime(0.016);
    }
    
    double massAfterPhysics = world->getTotalMass();
    double removedMassAfterPhysics = world->getRemovedMass();
    
    // Total system mass should be conserved (within tolerance)
    double totalSystemMass = massAfterPhysics + removedMassAfterPhysics;
    EXPECT_NEAR(totalSystemMass, massAfterAddition, 0.1);
    
    spdlog::info("Mass conservation validated for {} - initial: {}, after addition: {}, after physics: {}, removed: {}, total: {}", 
                 getWorldTypeName(), initialMass, massAfterAddition, massAfterPhysics, removedMassAfterPhysics, totalSystemMass);
}

// =================================================================
// PARAMETERIZED TEST INSTANTIATION
// =================================================================

INSTANTIATE_TEST_SUITE_P(
    BothWorldTypes,
    InterfaceCompatibilityTest,
    ::testing::Values(WorldType::RulesA, WorldType::RulesB),
    [](const ::testing::TestParamInfo<WorldType>& info) {
        switch (info.param) {
            case WorldType::RulesA: return "World_RulesA";
            case WorldType::RulesB: return "WorldB_RulesB";
            default: return "Unknown";
        }
    }
);