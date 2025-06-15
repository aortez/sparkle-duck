#include "visual_test_runner.h"
#include "../WorldB.h"
#include "../MaterialType.h"
#include <spdlog/spdlog.h>

class WorldBVisualTest : public VisualTestBase {
protected:
    void SetUp() override {
        VisualTestBase::SetUp();
        
        // Default to 3x3 world, but tests can override these
        width = 3;
        height = 3;
        createTestWorldB();
    }
    
    void createTestWorldB() {
        // Create small world for testing with no draw area (tests don't need UI)
        world = std::make_unique<WorldB>(width, height, nullptr);
        world->setAddParticlesEnabled(false);
        // Disable walls for testing to get clean mass calculations
        world->setWallsEnabled(false);
        world->reset(); // Reset to clear any walls that were created
    }

    void TearDown() override {
        world.reset();
        VisualTestBase::TearDown();
    }
    
    // Test data members
    std::unique_ptr<WorldB> world;
    uint32_t width;
    uint32_t height;
};

TEST_F(WorldBVisualTest, EmptyWorldAdvance) {
    spdlog::info("Starting WorldBVisualTest::EmptyWorldAdvance test");
    
    // Verify world is initially empty
    EXPECT_EQ(world->getWidth(), 3);
    EXPECT_EQ(world->getHeight(), 3);
    EXPECT_NEAR(world->getTotalMass(), 0.0, 0.001);
    
    // Advance time should work on empty world
    world->advanceTime(0.016);
    
    // Mass should still be zero
    EXPECT_NEAR(world->getTotalMass(), 0.0, 0.001);
}

TEST_F(WorldBVisualTest, MaterialInitialization) {
    spdlog::info("Starting WorldBVisualTest::MaterialInitialization test");
    
    // Test all material types can be added
    std::vector<MaterialType> materials = {
        MaterialType::DIRT,
        MaterialType::WATER, 
        MaterialType::WOOD,
        MaterialType::SAND,
        MaterialType::METAL,
        MaterialType::LEAF
    };
    
    double expectedMass = 0.0;
    
    for (size_t i = 0; i < materials.size() && i < 6; ++i) {
        MaterialType mat = materials[i];
        int x = i % 3;
        int y = i / 3;
        
        // Add material at cell coordinates
        world->addMaterialAtCell(x, y, mat);
        expectedMass += getMaterialDensity(mat);
        
        spdlog::info("Added {} at ({},{}) - density: {}", 
                     getMaterialName(mat), x, y, getMaterialDensity(mat));
    }
    
    // Verify total mass matches expected
    double actualMass = world->getTotalMass();
    EXPECT_NEAR(actualMass, expectedMass, 0.1);
    
    spdlog::info("Expected mass: {}, Actual mass: {}", expectedMass, actualMass);
}

TEST_F(WorldBVisualTest, BasicGravity) {
    spdlog::info("Starting WorldBVisualTest::BasicGravity test");
    
    // Create 3x3 world and add dirt at top
    world->addMaterialAtCell(1, 0, MaterialType::DIRT);
    
    double initialMass = world->getTotalMass();
    EXPECT_NEAR(initialMass, getMaterialDensity(MaterialType::DIRT), 0.1);
    
    // Advance time to let gravity work
    for (int i = 0; i < 10; ++i) {
        world->advanceTime(0.016);
    }
    
    // Mass should be conserved
    double finalMass = world->getTotalMass();
    EXPECT_NEAR(finalMass, initialMass, 0.1);
    
    spdlog::info("Initial mass: {}, Final mass: {}", initialMass, finalMass);
}

TEST_F(WorldBVisualTest, MaterialProperties) {
    spdlog::info("Starting WorldBVisualTest::MaterialProperties test");
    
    // Test material properties are correct
    EXPECT_NEAR(getMaterialDensity(MaterialType::AIR), 0.001, 0.0001);
    EXPECT_NEAR(getMaterialDensity(MaterialType::DIRT), 1.5, 0.1);
    EXPECT_NEAR(getMaterialDensity(MaterialType::WATER), 1.0, 0.1);
    EXPECT_NEAR(getMaterialDensity(MaterialType::WOOD), 0.8, 0.1);
    EXPECT_NEAR(getMaterialDensity(MaterialType::SAND), 1.8, 0.1);
    EXPECT_NEAR(getMaterialDensity(MaterialType::METAL), 7.8, 0.1);
    EXPECT_NEAR(getMaterialDensity(MaterialType::LEAF), 0.3, 0.1);
    EXPECT_NEAR(getMaterialDensity(MaterialType::WALL), 1000.0, 1.0);
    
    // Test fluid properties
    EXPECT_TRUE(isMaterialFluid(MaterialType::AIR));
    EXPECT_FALSE(isMaterialFluid(MaterialType::DIRT));
    EXPECT_TRUE(isMaterialFluid(MaterialType::WATER));
    EXPECT_FALSE(isMaterialFluid(MaterialType::WOOD));
    
    // Test rigid properties
    EXPECT_FALSE(isMaterialRigid(MaterialType::AIR));
    EXPECT_FALSE(isMaterialRigid(MaterialType::DIRT));
    EXPECT_FALSE(isMaterialRigid(MaterialType::WATER));
    EXPECT_TRUE(isMaterialRigid(MaterialType::WOOD));
    EXPECT_TRUE(isMaterialRigid(MaterialType::METAL));
    EXPECT_TRUE(isMaterialRigid(MaterialType::WALL));
}

TEST_F(WorldBVisualTest, VelocityLimiting) {
    spdlog::info("Starting WorldBVisualTest::VelocityLimiting test");
    
    // Test that WorldB implements velocity limiting as designed
    // This is more of a functionality check than physics validation
    
    // Add material 
    world->addMaterialAtCell(1, 1, MaterialType::DIRT);
    
    // Advance several timesteps
    for (int i = 0; i < 20; ++i) {
        world->advanceTime(0.016);
    }
    
    // WorldB should handle velocity limiting internally
    // We can't directly test velocities since they're internal to CellB
    // But we can verify the world still functions properly
    double mass = world->getTotalMass();
    EXPECT_GT(mass, 0.0);
    
    spdlog::info("Mass after velocity limiting test: {}", mass);
}

TEST_F(WorldBVisualTest, ResetFunctionality) {
    spdlog::info("Starting WorldBVisualTest::ResetFunctionality test");
    
    // Add some materials
    world->addMaterialAtCell(0, 0, MaterialType::DIRT);
    world->addMaterialAtCell(2, 2, MaterialType::WATER);
    
    double massBeforeReset = world->getTotalMass();
    EXPECT_GT(massBeforeReset, 0.0);
    
    // Reset the world
    world->reset();
    
    // World should be empty after reset
    double massAfterReset = world->getTotalMass();
    EXPECT_NEAR(massAfterReset, 0.0, 0.001);
    
    spdlog::info("Mass before reset: {}, after reset: {}", massBeforeReset, massAfterReset);
}

TEST_F(WorldBVisualTest, VelocityBehaviorTimestepCorrectness) {
    spdlog::info("Starting WorldBVisualTest::VelocityBehaviorTimestepCorrectness test");
    
    // Override to create a 1x4 world for this test
    width = 4;
    height = 1;
    createTestWorldB();
    
    // Turn off gravity for pure velocity testing
    world->setGravity(0.0);
    
    // Clear the world first (reset() may have added default materials)
    for (uint32_t y = 0; y < height; y++) {
        for (uint32_t x = 0; x < width; x++) {
            CellB& cell = world->at(x, y);
            cell.clear();
        }
    }
    
    // Add dirt particle at leftmost cell (0,0) with rightward velocity
    world->addMaterialAtCell(0, 0, MaterialType::DIRT);
    
    // Get initial cell and set a known velocity
    CellB& startCell = world->at(0, 0);
    ASSERT_FALSE(startCell.isEmpty()) << "Start cell should have dirt material";
    
    // Set a controlled velocity: 1.0 cells per second rightward
    const double velocityX = 1.0;  // cells/second
    const double velocityY = 0.0;  // no vertical movement
    startCell.setVelocity(Vector2d(velocityX, velocityY));
    
    spdlog::info("Initial setup: dirt at (0,0) with velocity ({}, {})", velocityX, velocityY);
    
    // Calculate expected travel time and steps
    const double distance = 3.0;  // cells to travel (from x=0 to x=3)
    const double expectedTimeSeconds = distance / velocityX;  // should be 3.0 seconds
    const double deltaTime = 0.016;  // 60 FPS timestep
    const int expectedSteps = static_cast<int>(expectedTimeSeconds / deltaTime);
    
    spdlog::info("Expected: {} seconds, {} steps to travel {} cells", 
                 expectedTimeSeconds, expectedSteps, distance);
    
    // Test deltaTime fix by checking time to reach first boundary  
    // With corrected transfer logic (transfers at COM=±1.0), particle needs to travel
    // 2.0 COM units (from 0 to 1.0) at velocity 1.0, so ~2 seconds (125 steps)
    const double expectedTimeToFirstBoundary = 2.0;  // seconds  
    const int expectedStepsToFirstBoundary = static_cast<int>(expectedTimeToFirstBoundary / deltaTime);
    
    int stepsToFirstTransfer = 0;
    bool reachedFirstBoundary = false;
    
    // Track particle until it leaves cell (0,0)
    for (stepsToFirstTransfer = 0; stepsToFirstTransfer < expectedStepsToFirstBoundary * 2; ++stepsToFirstTransfer) {
        world->advanceTime(deltaTime);
        
        // Check if particle has moved to cell (1,0)
        CellB& firstCell = world->at(0, 0);
        CellB& secondCell = world->at(1, 0);
        
        if (firstCell.isEmpty() || !secondCell.isEmpty()) {
            reachedFirstBoundary = true;
            spdlog::info("Particle reached first boundary after {} steps", stepsToFirstTransfer + 1);
            break;
        }
        
        // Log progress every 20 steps
        if ((stepsToFirstTransfer + 1) % 20 == 0) {
            Vector2d com = firstCell.getCOM();
            Vector2d velocity = firstCell.getVelocity();
            spdlog::info("Step {}: COM=({:.3f},{:.3f}), velocity=({:.3f},{:.3f})",
                         stepsToFirstTransfer + 1, com.x, com.y, velocity.x, velocity.y);
        }
    }
    
    ASSERT_TRUE(reachedFirstBoundary) << "Particle should have crossed first boundary within " 
                                      << expectedStepsToFirstBoundary * 2 << " steps";
    
    // Verify deltaTime integration and correct transfer threshold (1.0)
    const double tolerance = 0.20;  // 20% tolerance for first boundary crossing
    const int stepTolerance = static_cast<int>(expectedStepsToFirstBoundary * tolerance);
    
    spdlog::info("DeltaTime integration and transfer threshold test results:");
    spdlog::info("  Expected steps to reach COM=1.0: {} ± {} (20% tolerance)", 
                 expectedStepsToFirstBoundary, stepTolerance);
    spdlog::info("  Actual steps to first transfer: {}", stepsToFirstTransfer + 1);
    spdlog::info("  Difference: {} steps", abs((stepsToFirstTransfer + 1) - expectedStepsToFirstBoundary));
    
    // This test verifies both deltaTime integration and correct transfer threshold
    // The particle should take ~2 seconds to reach COM=1.0 and trigger transfer
    EXPECT_NEAR(stepsToFirstTransfer + 1, expectedStepsToFirstBoundary, stepTolerance) 
        << "Particle should reach transfer boundary (COM=1.0) in approximately correct time. "
        << "This verifies both deltaTime integration and transfer threshold correctness.";
}

TEST_F(WorldBVisualTest, DirtMetalCollisionReflection) {
    spdlog::info("Starting WorldBVisualTest::DirtMetalCollisionReflection test");
    
    // Create 3x1 world: Dirt - Empty - Metal
    width = 3;
    height = 1;
    createTestWorldB();
    
    // Turn off gravity to focus on collision physics
    world->setGravity(0.0);
    
    // Clear the world first
    for (uint32_t x = 0; x < width; x++) {
        world->at(x, 0).clear();
    }
    
    // Setup: Dirt on left (0,0), Empty in middle (1,0), Metal on right (2,0)
    world->addMaterialAtCell(0, 0, MaterialType::DIRT);
    world->addMaterialAtCell(2, 0, MaterialType::METAL);
    
    // Give dirt particle rightward velocity toward metal
    CellB& dirtCell = world->at(0, 0);
    const double initialVelocity = 2.0; // cells/second rightward
    dirtCell.setVelocity(Vector2d(initialVelocity, 0.0));
    
    spdlog::info("Initial setup: Dirt at (0,0) with velocity {}, Metal at (2,0)", initialVelocity);
    
    // Track particle movement and look for collision/reflection
    bool collisionDetected = false;
    bool reflectionDetected = false;
    Vector2d dirtVelocityBeforeCollision(0, 0);
    Vector2d dirtVelocityAfterCollision(0, 0);
    
    const double deltaTime = 0.016;
    const int maxSteps = 500; // Generous limit
    
    for (int step = 0; step < maxSteps; step++) {
        // Record state before timestep
        CellB& cell0 = world->at(0, 0);
        CellB& cell1 = world->at(1, 0);
        CellB& cell2 = world->at(2, 0);
        
        // Check if dirt has moved to middle cell (collision imminent)
        if (!cell1.isEmpty() && cell1.getMaterialType() == MaterialType::DIRT && !collisionDetected) {
            dirtVelocityBeforeCollision = cell1.getVelocity();
            collisionDetected = true;
            spdlog::info("Step {}: Dirt moved to middle cell (1,0), velocity before collision: ({:.3f},{:.3f})", 
                         step, dirtVelocityBeforeCollision.x, dirtVelocityBeforeCollision.y);
        }
        
        world->advanceTime(deltaTime);
        
        // Check for reflection: dirt should bounce back with negative velocity
        if (collisionDetected && !reflectionDetected) {
            if (!cell0.isEmpty() && cell0.getMaterialType() == MaterialType::DIRT) {
                Vector2d currentVelocity = cell0.getVelocity();
                if (currentVelocity.x < 0.0) { // Reflected (negative x velocity)
                    dirtVelocityAfterCollision = currentVelocity;
                    reflectionDetected = true;
                    spdlog::info("Step {}: Reflection detected! Dirt back at (0,0) with velocity ({:.3f},{:.3f})", 
                                 step, currentVelocity.x, currentVelocity.y);
                    break;
                }
            }
        }
        
        // Log progress every 25 steps
        if (step % 25 == 0) {
            spdlog::info("Step {}: Cell (0,0): {} | Cell (1,0): {} | Cell (2,0): {}", 
                         step,
                         cell0.isEmpty() ? "Empty" : getMaterialName(cell0.getMaterialType()),
                         cell1.isEmpty() ? "Empty" : getMaterialName(cell1.getMaterialType()),
                         cell2.isEmpty() ? "Empty" : getMaterialName(cell2.getMaterialType()));
        }
    }
    
    spdlog::info("Collision test results:");
    spdlog::info("  Collision detected: {}", collisionDetected ? "YES" : "NO");
    spdlog::info("  Reflection detected: {}", reflectionDetected ? "YES" : "NO");
    
    if (collisionDetected) {
        spdlog::info("  Velocity before collision: ({:.3f},{:.3f})", 
                     dirtVelocityBeforeCollision.x, dirtVelocityBeforeCollision.y);
    }
    if (reflectionDetected) {
        spdlog::info("  Velocity after reflection: ({:.3f},{:.3f})", 
                     dirtVelocityAfterCollision.x, dirtVelocityAfterCollision.y);
        spdlog::info("  Velocity change: {:.3f} -> {:.3f} (ratio: {:.3f})", 
                     dirtVelocityBeforeCollision.x, dirtVelocityAfterCollision.x,
                     dirtVelocityAfterCollision.x / dirtVelocityBeforeCollision.x);
    }
    
    // Verify collision behavior
    EXPECT_TRUE(collisionDetected) << "Dirt should move and reach the middle cell before collision";
    EXPECT_TRUE(reflectionDetected) << "Dirt should reflect off metal wall with negative velocity";
    
    if (reflectionDetected) {
        // Check that reflection has proper physics:
        // 1. Velocity should be reversed (negative)
        EXPECT_LT(dirtVelocityAfterCollision.x, 0.0) << "Reflected velocity should be negative (leftward)";
        
        // 2. Energy should be partially conserved (elastic reflection with some loss)
        double energyRatio = std::abs(dirtVelocityAfterCollision.x / dirtVelocityBeforeCollision.x);
        EXPECT_GT(energyRatio, 0.1) << "Reflection should preserve significant energy (> 10%)";
        EXPECT_LT(energyRatio, 1.0) << "Reflection should lose some energy (< 100%)";
        
        spdlog::info("Energy conservation ratio: {:.3f} (should be between 0.1 and 1.0)", energyRatio);
    }
}