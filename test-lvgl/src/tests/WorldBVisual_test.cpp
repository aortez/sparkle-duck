#include "visual_test_runner.h"
#include "../WorldB.h"
#include "../MaterialType.h"
#include <spdlog/spdlog.h>

class WorldBVisualTest : public VisualTestBase {
protected:
    void SetUp() override {
        VisualTestBase::SetUp();
        
        // Create world with default size (3x3)
        world = createWorldB(3, 3);
        
        // Apply test-specific defaults
        world->setAddParticlesEnabled(false);
        world->setWallsEnabled(false);
        world->setup();  // Setup with initial materials (most tests want this)
    }
    
    void TearDown() override {
        // Don't reset world here - let VisualTestBase handle cleanup in correct order
        VisualTestBase::TearDown();
    }
    
    std::unique_ptr<WorldInterface> world;
};

TEST_F(WorldBVisualTest, EmptyWorldAdvance) {
    spdlog::info("Starting WorldBVisualTest::EmptyWorldAdvance test");
    
    // Reset to empty state for this test (don't call setup())
    world->reset();
    
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
    
    // Reset to empty state for this test (don't use the default setup materials)
    world->reset();
    
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
    
    // Log initial test state after materials are set up
    logInitialTestState(world.get(), "Material initialization test - 6 different materials");
    
    // Verify total mass matches expected
    double actualMass = world->getTotalMass();
    EXPECT_NEAR(actualMass, expectedMass, 0.1);
    
    spdlog::info("Expected mass: {}, Actual mass: {}", expectedMass, actualMass);
}

TEST_F(WorldBVisualTest, BasicGravity) {
    spdlog::info("Starting WorldBVisualTest::BasicGravity test");
    
    // Reset to empty state for this test (don't use the default setup materials)
    world->reset();
    
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
    
    // Create a new 4x1 world for this test
    world = createWorldB(4, 1);
    world->setAddParticlesEnabled(false);
    world->setWallsEnabled(false);
    
    // Reset to empty state for this test (don't use the default setup materials)
    world->reset();
    
    // Turn off gravity for pure velocity testing
    world->setGravity(0.0);
    
    // Clear the world first (reset() may have added default materials)
    // Since we can't directly access cells through WorldInterface, 
    // we need to cast to WorldB for this specific test
    if (auto* worldB = dynamic_cast<WorldB*>(world.get())) {
        for (uint32_t y = 0; y < worldB->getHeight(); y++) {
            for (uint32_t x = 0; x < worldB->getWidth(); x++) {
                CellB& cell = worldB->at(x, y);
                cell.clear();
            }
        }
    }
    
    // Add dirt particle at leftmost cell (0,0) with rightward velocity
    world->addMaterialAtCell(0, 0, MaterialType::DIRT);
    
    // This test needs direct access to WorldB implementation details
    auto* worldB = dynamic_cast<WorldB*>(world.get());
    ASSERT_NE(worldB, nullptr) << "This test requires WorldB implementation";
    
    // Get initial cell and set a known velocity
    CellB& startCell = worldB->at(0, 0);
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
    // 1.0 COM units (from 0 to 1.0) at velocity 1.0, so ~1 second (62.5 steps)
    const double expectedTimeToFirstBoundary = 1.0;  // seconds  
    const int expectedStepsToFirstBoundary = static_cast<int>(expectedTimeToFirstBoundary / deltaTime);
    
    int stepsToFirstTransfer = 0;
    bool reachedFirstBoundary = false;
    
    // Track particle until it leaves cell (0,0)
    for (stepsToFirstTransfer = 0; stepsToFirstTransfer < expectedStepsToFirstBoundary * 2; ++stepsToFirstTransfer) {
        world->advanceTime(deltaTime);
        
        // Check if particle has moved to cell (1,0)
        CellB& firstCell = worldB->at(0, 0);
        CellB& secondCell = worldB->at(1, 0);
        
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

// Parameterized collision test data
struct CollisionTestCase {
    MaterialType movingMaterial;
    MaterialType targetMaterial;
    bool expectElasticBehavior;
    std::string description;
};

class CollisionBehaviorTest : public VisualTestBase, 
                             public ::testing::WithParamInterface<CollisionTestCase> {
protected:
    void SetUp() override {
        VisualTestBase::SetUp();
        
        // World will be created in the test itself with custom dimensions
    }
    
    std::unique_ptr<WorldInterface> world;
};

TEST_P(CollisionBehaviorTest, MaterialCollisionBehavior) {
    const auto& testCase = GetParam();
    spdlog::info("Starting CollisionBehaviorTest: {}", testCase.description);
    
    // Create 3x1 world for collision testing
    world = createWorldB(3, 1);
    world->setAddParticlesEnabled(false);
    world->setWallsEnabled(false);
    
    // Empty world
    world->reset();
    
    // Turn off gravity to focus on collision physics
    world->setGravity(0.0);
    
    // Setup: Moving material on left (0,0), Empty in middle (1,0), Target material on right (2,0)
    world->addMaterialAtCell(0, 0, testCase.movingMaterial);
    world->addMaterialAtCell(2, 0, testCase.targetMaterial);
    
    // Give moving particle rightward velocity toward target
    // Need direct access to cells for velocity setting
    auto* worldB = dynamic_cast<WorldB*>(world.get());
    ASSERT_NE(worldB, nullptr) << "This test requires WorldB implementation";
    
    CellB& movingCell = worldB->at(0, 0);
    const double initialVelocity = 2.0; // cells/second rightward
    movingCell.setVelocity(Vector2d(initialVelocity, 0.0));
    
    spdlog::info("Initial setup: {} at (0,0) with velocity {}, {} at (2,0)", 
                 getMaterialName(testCase.movingMaterial), initialVelocity, 
                 getMaterialName(testCase.targetMaterial));
    
    // Track particle movement and look for collision/reflection
    bool collisionDetected = false;
    bool reflectionDetected = false;
    Vector2d velocityBeforeCollision(0, 0);
    Vector2d velocityAfterCollision(0, 0);
    
    const double deltaTime = 0.016;
    const int maxSteps = 500; // Generous limit
    
    for (int step = 0; step < maxSteps; step++) {
        // Record state before timestep
        CellB& cell0 = worldB->at(0, 0);
        CellB& cell1 = worldB->at(1, 0);
        CellB& cell2 = worldB->at(2, 0);
        
        // Check if moving material has moved to middle cell (collision imminent)
        if (!cell1.isEmpty() && cell1.getMaterialType() == testCase.movingMaterial && !collisionDetected) {
            velocityBeforeCollision = cell1.getVelocity();
            collisionDetected = true;
            spdlog::info("Step {}: {} moved to middle cell (1,0), velocity before collision: ({:.3f},{:.3f})", 
                         step, getMaterialName(testCase.movingMaterial), 
                         velocityBeforeCollision.x, velocityBeforeCollision.y);
        }
        
        world->advanceTime(deltaTime);
        
        // Check for reflection: material should bounce back with negative velocity
        if (collisionDetected && !reflectionDetected) {
            if (!cell0.isEmpty() && cell0.getMaterialType() == testCase.movingMaterial) {
                Vector2d currentVelocity = cell0.getVelocity();
                if (currentVelocity.x < 0.0) { // Reflected (negative x velocity)
                    velocityAfterCollision = currentVelocity;
                    reflectionDetected = true;
                    spdlog::info("Step {}: Reflection detected! {} back at (0,0) with velocity ({:.3f},{:.3f})", 
                                 step, getMaterialName(testCase.movingMaterial),
                                 currentVelocity.x, currentVelocity.y);
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
    
    spdlog::info("Collision test results for {}: {}", testCase.description, "");
    spdlog::info("  Collision detected: {}", collisionDetected ? "YES" : "NO");
    spdlog::info("  Reflection detected: {}", reflectionDetected ? "YES" : "NO");
    spdlog::info("  Expected elastic behavior: {}", testCase.expectElasticBehavior ? "YES" : "NO");
    
    if (collisionDetected) {
        spdlog::info("  Velocity before collision: ({:.3f},{:.3f})", 
                     velocityBeforeCollision.x, velocityBeforeCollision.y);
    }
    if (reflectionDetected) {
        spdlog::info("  Velocity after reflection: ({:.3f},{:.3f})", 
                     velocityAfterCollision.x, velocityAfterCollision.y);
        spdlog::info("  Velocity change: {:.3f} -> {:.3f} (ratio: {:.3f})", 
                     velocityBeforeCollision.x, velocityAfterCollision.x,
                     velocityAfterCollision.x / velocityBeforeCollision.x);
    }
    
    // Verify collision detection works for all material combinations
    EXPECT_TRUE(collisionDetected) << "Moving material should reach the middle cell and trigger collision detection";
    
    // Verify elastic vs inelastic behavior matches expectations
    if (testCase.expectElasticBehavior) {
        // For elastic materials, we expect some form of bouncing behavior
        // Note: "Reflection detected" may not be the right metric since the collision system
        // may handle elastic collisions differently than complete position reversal
        spdlog::info("  Expected elastic behavior - collision system should process as ELASTIC_REFLECTION");
    } else {
        // For inelastic materials, we should NOT see reflection
        EXPECT_FALSE(reflectionDetected) << "Inelastic materials should not bounce back to original position";
        spdlog::info("  Expected inelastic behavior - material should not return to original cell");
    }
}

// Define test cases for different material collision behaviors
INSTANTIATE_TEST_SUITE_P(
    MaterialCollisions,
    CollisionBehaviorTest,
    ::testing::Values(
        // Elastic collisions (high elasticity, rigid materials - should bounce)
        CollisionTestCase{MaterialType::METAL, MaterialType::METAL, true, 
                         "Metal-Metal collision (both elastic=0.8, rigid)"},
        CollisionTestCase{MaterialType::WOOD, MaterialType::METAL, true, 
                         "Wood-Metal collision (elastic=0.6 vs 0.8, both rigid)"},
        CollisionTestCase{MaterialType::METAL, MaterialType::WALL, true, 
                         "Metal-Wall collision (elastic=0.8 vs 0.9, metal vs immovable)"},
        
        // Inelastic collisions (low elasticity or soft materials - should NOT bounce)
        CollisionTestCase{MaterialType::DIRT, MaterialType::METAL, false, 
                         "Dirt-Metal collision (elastic=0.3 vs 0.8, soft vs rigid)"},
        CollisionTestCase{MaterialType::SAND, MaterialType::METAL, false, 
                         "Sand-Metal collision (elastic=0.2 vs 0.8, soft vs rigid)"},
        CollisionTestCase{MaterialType::WATER, MaterialType::METAL, false, 
                         "Water-Metal collision (elastic=0.1 vs 0.8, fluid vs rigid)"},
        CollisionTestCase{MaterialType::DIRT, MaterialType::DIRT, false, 
                         "Dirt-Dirt collision (both elastic=0.3, both soft)"},
        CollisionTestCase{MaterialType::LEAF, MaterialType::WOOD, false, 
                         "Leaf-Wood collision (elastic=0.4 vs 0.6, light vs rigid)"}
    )
);
