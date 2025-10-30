#include "visual_test_runner.h"
#include "../World.h"
#include "../Cell.h"
#include "../MaterialType.h"
#include "../Vector2d.h"
#include "spdlog/spdlog.h"
#include "spdlog/fmt/fmt.h"
#include <sstream>
#include <iomanip>

class CollisionSystemTest : public VisualTestBase {
protected:
    void SetUp() override {
        // Call parent SetUp first.
        VisualTestBase::SetUp();
        
        // Ensure restart is disabled for collision tests.
        disableTestRestart();
        
        // Create world with desired size using framework method.
        world = createWorldB(5, 5);
        
        // Apply test-specific defaults.
        world->setAddParticlesEnabled(false);
        world->setWallsEnabled(false);
        
        // DON'T call world->setup() - we want a completely clean world.
        // Instead, manually clear all cells to ensure they're empty.
        cleanWorldForTesting();
        
        // Disable gravity for collision tests.
        world->setGravity(0.0);
        world->setRainRate(0.0);
        
        // Set up logging to see detailed collision output.
        spdlog::set_level(spdlog::level::debug);
    }
    
    void TearDown() override {
        // Call parent TearDown first (it may need to access the world).
        VisualTestBase::TearDown();
        // Then clean up our world.
        world.reset();
    }
    
    // REQUIRED: Override for unified simulation loop pattern.
    WorldInterface* getWorldInterface() override {
        return world.get();
    }
    
    // Clean world for testing without any materials.
    void cleanWorldForTesting() {
        for (uint32_t y = 0; y < world->getHeight(); ++y) {
            for (uint32_t x = 0; x < world->getWidth(); ++x) {
                Cell& cell = world->at(x, y);
                cell.clear(); // Set to AIR with no velocity/COM.
            }
        }
        // Clear any pending moves.
        world->clearPendingMoves();
    }
    
    // Helper to set up a cell with specific properties.
    void setupCell(uint32_t x, uint32_t y, MaterialType material, double fillRatio, 
                   Vector2d com = Vector2d(0.0, 0.0), Vector2d velocity = Vector2d(0.0, 0.0)) {
        Cell& cell = world->at(x, y);
        cell.setMaterialType(material);
        cell.setFillRatio(fillRatio);
        cell.setCOM(com);
        cell.setVelocity(velocity);
    }
    
    std::unique_ptr<World> world;
};

TEST_F(CollisionSystemTest, ParticleCrossesCellBoundaries) {
    // Enable restart functionality for this test.
    runRestartableTest([this]() {
        // Create a smaller 3x1 world for this specific test.
        world = createWorldB(3, 1);
        world->setAddParticlesEnabled(false);
        world->setWallsEnabled(false);
        world->setGravity(0.0);  // No gravity for pure velocity test.
        cleanWorldForTesting();
    
    // Add DIRT particle at left cell with high rightward velocity.
    setupCell(0, 0, MaterialType::DIRT, 1.0, Vector2d(0.0, 0.0), Vector2d(10.0, 0.0));
    
    // Log initial state.
    logWorldState(world.get(), "Initial Setup: DIRT at left with rightward velocity");
    
    // Show initial state with Step functionality for detailed observation.
    showInitialStateWithStep(world.get(), "DIRT particle at left with velocity 5.0 cells/s rightward");
    
    // State tracking variables.
    bool reachedMiddle = false;
    bool reachedRight = false;
    int middleReachedStep = -1;
    int rightReachedStep = -1;
    
    // Use unified simulation loop.
    runSimulationLoop(50, [&](int step) {
        // Log world state at each step.
        logWorldState(world.get(), fmt::format("ParticleCrossesCellBoundaries - Step {}", step + 1));
        
        // Check if particle reached middle cell.
        if (!reachedMiddle && !world->at(1, 0).isEmpty()) {
            reachedMiddle = true;
            middleReachedStep = step;
        }
        
        // Check if particle reached right cell.
        if (!reachedRight && !world->at(2, 0).isEmpty()) {
            reachedRight = true;
            rightReachedStep = step;
        }
        
        // Visual mode status update.
        if (visual_mode_) {
            std::stringstream ss;
            ss << "Step " << step + 1 << ": ";
            if (reachedRight) {
                ss << "Particle reached right cell!";
            } else if (reachedMiddle) {
                ss << "Particle in middle cell";
            } else {
                ss << "Particle in left cell";
            }
            updateDisplay(world.get(), ss.str());
        }
    },
    "Particle boundary crossing test",
    [&]() { return reachedRight; }  // Stop early when reached right cell.
    );
    
    // Verify results.
    ASSERT_TRUE(reachedMiddle) << "Particle should reach middle cell";
    ASSERT_TRUE(reachedRight) << "Particle should reach right cell";
    
    // Verify timing is reasonable (COM travels from 0 to 1.0 at 5.0 cells/s = 0.2s = ~12 steps).
    EXPECT_LT(middleReachedStep, 20) << "High velocity particle should cross boundary quickly";
    
        if (visual_mode_) {
            updateDisplay(world.get(), "Test complete - particle crossed both boundaries");
            waitForRestartOrNext();
        }
    });  // End of runRestartableTest.
}

TEST_F(CollisionSystemTest, ElasticCollisionBetweenMetals) {
    // Setup: METAL particle moving toward another METAL particle.
    setupCell(1, 2, MaterialType::METAL, 1.0, Vector2d(0.9, 0.0), Vector2d(2, 0.0));
    setupCell(2, 2, MaterialType::METAL, 0.8, Vector2d(0.0, 0.0), Vector2d(0.0, 0.0));
    
    // Log initial state.
    logWorldState(world.get(), "Initial Setup: Two METAL particles for collision");
    
    showInitialStateWithStep(world.get(), "METAL particles: left moving right, right stationary");
    
    // Store initial velocities.
    Vector2d v1_initial = world->at(1, 2).getVelocity();
    Vector2d v2_initial = world->at(2, 2).getVelocity();
    
    // State tracking.
    bool collisionDetected = false;
    int collisionStep = -1;
    Vector2d v1_final, v2_final;
    int postCollisionSteps = 0;
    Vector2d v1_post, v2_post;
    
    // Use unified simulation loop.
    runSimulationLoop(100, [&](int step) {
        // Log world state at each step.
        logWorldState(world.get(), fmt::format("ElasticCollision - Step {}", step + 1));
        
        Vector2d v1_current = world->at(1, 2).getVelocity();
        Vector2d v2_current = world->at(2, 2).getVelocity();
        
        // Check if velocities changed significantly.
        if (!collisionDetected && 
            (std::abs(v1_current.x - v1_initial.x) > 0.01 || 
             std::abs(v2_current.x - v2_initial.x) > 0.01)) {
            collisionDetected = true;
            collisionStep = step;
            v1_final = v1_current;
            v2_final = v2_current;
        }
        
        // Track post-collision behavior.
        if (collisionDetected) {
            postCollisionSteps++;
            v1_post = v1_current;
            v2_post = v2_current;
            
            // Verify velocities remain stable after collision.
            if (postCollisionSteps > 1) {
                EXPECT_NEAR(v1_post.x, v1_final.x, 0.01) 
                    << "Particle 1 velocity should remain stable after collision";
                EXPECT_NEAR(v2_post.x, v2_final.x, 0.01) 
                    << "Particle 2 velocity should remain stable after collision";
            }
        }
        
        // Visual status update.
        if (visual_mode_) {
            std::stringstream ss;
            ss << "Step " << step + 1 << "\n";
            ss << "v1: (" << std::fixed << std::setprecision(3) << v1_current.x << ", 0)\n";
            ss << "v2: (" << v2_current.x << ", 0)\n";
            if (collisionDetected) {
                ss << "Collision detected at step " << collisionStep << "!\n";
                ss << "Post-collision steps: " << postCollisionSteps;
            }
            updateDisplay(world.get(), ss.str());
        }
    },
    "Elastic collision detection with post-collision verification",
    [&]() { return collisionDetected && postCollisionSteps >= 5; }  // Continue for 5 steps after collision.
    );
    
    // Verify collision occurred.
    ASSERT_TRUE(collisionDetected) << "Elastic collision should occur between METAL particles";
    
    // Verify basic collision response.
    EXPECT_NE(v1_final.x, v1_initial.x) << "First particle velocity should change";
    EXPECT_NE(v2_final.x, v2_initial.x) << "Second particle velocity should change";
    
    // Verify expected elastic collision results.
    // For elastic collision with one particle at rest:
    // v1' should be close to 0 (moving particle transfers most momentum).
    // v2' should be close to v1_initial * elasticity.
    EXPECT_LT(std::abs(v1_final.x), 0.5) 
        << "Moving particle should slow down significantly after collision";
    EXPECT_GT(v2_final.x, 1.0) 
        << "Stationary particle should gain significant velocity";
    
    // The exact values depend on masses (fill ratios) and elasticity.
    // With elasticity = 0.8, expect some energy loss.
    double initial_kinetic_energy = 0.5 * v1_initial.x * v1_initial.x;
    double final_kinetic_energy = 0.5 * (v1_final.x * v1_final.x + v2_final.x * v2_final.x);
    double energy_ratio = final_kinetic_energy / initial_kinetic_energy;
    
    // With elasticity 0.8, expect about 64% energy retention (0.8^2).
    EXPECT_GT(energy_ratio, 0.5) << "Should retain at least 50% of kinetic energy";
    EXPECT_LT(energy_ratio, 0.9) << "Should lose some energy (not perfectly elastic)";
    
    if (visual_mode_) {
        std::stringstream ss;
        ss << "Elastic collision complete\n";
        ss << "Initial: v1=" << v1_initial.x << " v2=" << v2_initial.x << "\n";
        ss << "Final: v1=" << v1_final.x << " v2=" << v2_final.x << "\n";
        ss << "Energy retention: " << std::fixed << std::setprecision(1) 
           << energy_ratio * 100 << "%";
        updateDisplay(world.get(), ss.str());
        waitForNext();
    }
}

TEST_F(CollisionSystemTest, DiagonalMovementCrossesMultipleBoundaries) {
    // Setup: Particle moving diagonally to cross both X and Y boundaries.
    setupCell(2, 2, MaterialType::SAND, 1.0, Vector2d(0.8, 0.7), Vector2d(0.5, 0.6));
    
    // Log initial state.
    logWorldState(world.get(), "Initial Setup: SAND particle with diagonal velocity");
    
    showInitialStateWithStep(world.get(), "SAND particle moving diagonally (right and down)");
    
    // Track movement through cells - check initial state.
    bool startedAt22 = !world->at(2, 2).isEmpty();
    bool appearedIn32 = false;
    bool appearedIn23 = false;
    bool endedIn33 = false;
    int completedAtStep = -1;
    
    // Use unified simulation loop.
    runSimulationLoop(50, [&](int step) {
        // Log world state at each step.
        logWorldState(world.get(), fmt::format("DiagonalMovement - Step {}", step + 1));
        
        // Check all relevant cells.
        if (!world->at(3, 2).isEmpty()) {
            appearedIn32 = true;
        }
        if (!world->at(2, 3).isEmpty()) {
            appearedIn23 = true;
        }
        if (!world->at(3, 3).isEmpty() && !endedIn33) {
            endedIn33 = true;
            completedAtStep = step;
        }
        
        // Visual status update.
        if (visual_mode_) {
            std::stringstream ss;
            ss << "Step " << step + 1 << ": ";
            
            // Find where the particle currently is.
            bool found = false;
            for (int y = 2; y <= 3 && !found; y++) {
                for (int x = 2; x <= 3 && !found; x++) {
                    if (!world->at(x, y).isEmpty()) {
                        ss << "SAND at (" << x << "," << y << ")";
                        found = true;
                    }
                }
            }
            if (!found) {
                ss << "Particle location unknown";
            }
            updateDisplay(world.get(), ss.str());
        }
    },
    "Diagonal movement test",
    [&]() { return endedIn33; }  // Stop when reached destination.
    );
    
    // Verify results.
    EXPECT_TRUE(startedAt22) << "Particle should start at (2,2)";
    EXPECT_TRUE(appearedIn32 || appearedIn23) 
        << "Particle should pass through either (3,2) or (2,3) when moving diagonally";
    EXPECT_TRUE(endedIn33) << "Particle should end up at (3,3) after diagonal movement";
    
    if (visual_mode_) {
        updateDisplay(world.get(), "Diagonal movement complete - particle reached (3,3)");
        waitForNext();
    }
}

TEST_F(CollisionSystemTest, ProcessTransferMove) {
    // Setup: DIRT particle moving into empty space with COM close to boundary.
    setupCell(2, 2, MaterialType::DIRT, 1.0, Vector2d(0.9, 0.0), Vector2d(0.5, 0.0));
    setupCell(3, 2, MaterialType::AIR, 0.0);
    
    // Store initial state.
    double initialFillRatio1 = world->at(2, 2).getFillRatio();
    double initialFillRatio2 = world->at(3, 2).getFillRatio();
    
    // Log initial state.
    logWorldState(world.get(), "Initial Setup: DIRT with COM near boundary");
    
    showInitialStateWithStep(world.get(), "DIRT particle with COM near boundary moving right");
    
    // State tracking.
    double finalFillRatio1, finalFillRatio2;
    MaterialType finalMaterial3_2;
    
    // Use unified simulation loop - single step with large timestep.
    runSimulationLoop(1, [&](int /* step */) {
        // Log world state.
        logWorldState(world.get(), "ProcessTransferMove - After physics step");
        
        // Use larger timestep to ensure boundary crossing.
        world->advanceTime(0.3 - 0.016);  // Additional time beyond normal step.
        
        finalFillRatio1 = world->at(2, 2).getFillRatio();
        finalFillRatio2 = world->at(3, 2).getFillRatio();
        finalMaterial3_2 = world->at(3, 2).getMaterialType();
        
        // Visual mode display.
        if (visual_mode_) {
            std::stringstream ss;
            ss << "Initial: Cell(2,2) fill=" << std::fixed << std::setprecision(3) 
               << initialFillRatio1 << ", Cell(3,2) fill=" << initialFillRatio2 << "\n";
            ss << "Final: Cell(2,2) fill=" << finalFillRatio1 
               << ", Cell(3,2) fill=" << finalFillRatio2 << "\n";
            if (finalFillRatio2 > initialFillRatio2) {
                ss << "Material successfully transferred!";
            }
            updateDisplay(world.get(), ss.str());
        }
    },
    "Material transfer test"
    );
    
    // Verify results.
    EXPECT_LT(finalFillRatio1, initialFillRatio1) << "Source should lose material";
    EXPECT_GT(finalFillRatio2, initialFillRatio2) << "Target should gain material";
    EXPECT_EQ(finalMaterial3_2, MaterialType::DIRT) << "Target should have correct material";
    
    if (visual_mode_) {
        waitForNext();
    }
}

TEST_F(CollisionSystemTest, PhysicsConservation) {
    // Setup: Test momentum conservation in elastic collision.
    // Use velocity within limits (max is 0.9) to avoid clamping issues.
    setupCell(1, 2, MaterialType::METAL, 1.0, Vector2d(0.9, 0.0), Vector2d(0.8, 0.0));
    setupCell(2, 2, MaterialType::METAL, 1.0, Vector2d(-0.2, 0.0), Vector2d(0.0, 0.0));
    
    // Log initial state.
    logWorldState(world.get(), "Initial Setup: METAL collision for momentum test");
    
    showInitialStateWithStep(world.get(), "Testing momentum conservation in METAL-METAL collision");
    
    // Calculate initial momentum (after any velocity limiting).
    double mass1 = getMaterialDensity(MaterialType::METAL) * 1.0;
    double mass2 = getMaterialDensity(MaterialType::METAL) * 1.0;
    Vector2d initialMomentum = world->at(1, 2).getVelocity() * mass1 + world->at(2, 2).getVelocity() * mass2;
    
    // State tracking.
    Vector2d finalMomentum;
    bool collisionOccurred = false;
    Vector2d v1_after, v2_after;
    
    // Use unified simulation loop with multiple steps to ensure collision.
    runSimulationLoop(10, [&](int step) {
        // Log world state.
        logWorldState(world.get(), fmt::format("PhysicsConservation - Step {}", step + 1));
        
        Vector2d v1 = world->at(1, 2).getVelocity();
        Vector2d v2 = world->at(2, 2).getVelocity();
        
        // Check if collision occurred (second particle gains velocity).
        if (!collisionOccurred && v2.length() > 0.1) {
            collisionOccurred = true;
            v1_after = v1;
            v2_after = v2;
            finalMomentum = v1 * mass1 + v2 * mass2;
        }
        
        // Visual mode display.
        if (visual_mode_) {
            std::stringstream ss;
            ss << "Step " << step + 1 << "\n";
            ss << "Initial momentum: (" << std::fixed << std::setprecision(3) 
               << initialMomentum.x << ", " << initialMomentum.y << ")\n";
            ss << "Current momentum: (" << (v1 * mass1 + v2 * mass2).x << ", " 
               << (v1 * mass1 + v2 * mass2).y << ")\n";
            ss << "Velocities: v1=(" << v1.x << ",0) v2=(" << v2.x << ",0)\n";
            if (collisionOccurred) {
                ss << "Collision detected at step " << step << "!";
            }
            updateDisplay(world.get(), ss.str());
        }
    },
    "Momentum conservation test",
    [&]() { return collisionOccurred; }  // Stop when collision occurs.
    );
    
    // Verify results.
    ASSERT_TRUE(collisionOccurred) << "Collision should occur between METAL particles";
    
    // Verify momentum is approximately conserved (with elasticity loss).
    // METAL has elasticity of 0.8, which affects energy but momentum should be conserved.
    // However, with our separation adjustment, there might be small momentum changes.
    double momentum_ratio = finalMomentum.x / initialMomentum.x;
    EXPECT_GT(momentum_ratio, 0.7) << "Momentum should be mostly conserved";
    EXPECT_LT(momentum_ratio, 1.1) << "Momentum shouldn't increase";
    
    // Verify collision produced expected velocity exchange.
    EXPECT_LT(v1_after.x, 0.5) << "First particle should slow down";
    EXPECT_GT(v2_after.x, 0.3) << "Second particle should speed up";
    
    if (visual_mode_) {
        waitForNext();
    }
}

TEST_F(CollisionSystemTest, ForceCollisionScenario) {
    // Setup: Put particle very close to boundary with higher velocity to ensure collision.
    setupCell(1, 2, MaterialType::METAL, 1.0, Vector2d(0.9, 0.0), Vector2d(0.8, 0.0)); // COM near boundary.
    setupCell(2, 2, MaterialType::METAL, 1.0, Vector2d(-0.2, 0.0), Vector2d(0.0, 0.0));
    
    // Log initial state.
    logWorldState(world.get(), "Initial Setup: METAL near boundary with velocity");
    
    showInitialStateWithStep(world.get(), "METAL particle near boundary (0.9, 0.0) moving right");
    
    // Store initial velocities.
    Vector2d initialV1 = world->at(1, 2).getVelocity();
    Vector2d initialV2 = world->at(2, 2).getVelocity();
    
    // State tracking.
    Vector2d finalV1, finalV2;
    bool collisionDetected = false;
    Vector2d finalCOM1;
    
    // Use unified simulation loop - particle should cross boundary within 10 steps.
    runSimulationLoop(10, [&](int step) {
        // Log world state.
        logWorldState(world.get(), fmt::format("ForceCollisionScenario - Step {}", step + 1));
        
        finalV1 = world->at(1, 2).getVelocity();
        finalV2 = world->at(2, 2).getVelocity();
        finalCOM1 = world->at(1, 2).getCOM();
        
        // Check if collision occurred (velocities changed significantly).
        if (!collisionDetected && 
            (std::abs(finalV2.x - initialV2.x) > 0.1 || 
             std::abs(finalV1.x - initialV1.x) > 0.1)) {
            collisionDetected = true;
        }
        
        // Visual mode display.
        if (visual_mode_) {
            std::stringstream ss;
            ss << "Step " << step + 1 << "\n";
            ss << "COM1: (" << std::fixed << std::setprecision(3) 
               << finalCOM1.x << ", " << finalCOM1.y << ")\n";
            ss << "v1: (" << finalV1.x << ",0) v2: (" << finalV2.x << ",0)\n";
            if (collisionDetected) {
                ss << "Collision detected!";
            }
            updateDisplay(world.get(), ss.str());
        }
    },
    "Forced collision test",
    [&]() { return collisionDetected; }  // Stop when collision detected.
    );
    
    // Verify that collision occurred.
    ASSERT_TRUE(collisionDetected) 
        << "Collision should occur when particle moves from 0.95 toward boundary";
    
    // Verify collision produced expected results.
    EXPECT_NE(finalV1.x, initialV1.x) << "First particle velocity should change";
    EXPECT_NE(finalV2.x, initialV2.x) << "Second particle velocity should change";
    
    // Verify COM separation worked (should be pulled back from boundary).
    EXPECT_LT(finalCOM1.x, 1.0) << "COM should be separated from boundary";
    EXPECT_GT(finalCOM1.x, 0.9) << "COM should still be near boundary";
    
    if (visual_mode_) {
        waitForNext();
    }
}
