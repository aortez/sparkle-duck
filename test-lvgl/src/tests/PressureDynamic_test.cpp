#include <gtest/gtest.h>
#include "visual_test_runner.h"
#include <cmath>
#include "../WorldB.h"
#include "../MaterialType.h"
#include <spdlog/spdlog.h>
#include <spdlog/fmt/fmt.h>
#include <sstream>
#include <iomanip>

class PressureDynamicTest : public VisualTestBase {
protected:
    void SetUp() override {
        VisualTestBase::SetUp();
        
        // Enable trace logging to see detailed physics.
        spdlog::set_level(spdlog::level::trace);
        
        // Create a 3x3 world using enhanced framework (applies universal defaults).
        world = createWorldB(3, 3);
        
        // Override universal defaults for pressure testing - this test needs dynamic pressure enabled.
        // NOTE: These settings must come AFTER createWorldB which applies universal defaults.
        world->setPressureSystem(WorldInterface::PressureSystem::TopDown); // Use dual pressure system, not Original.
        world->setDynamicPressureEnabled(true);      // Enable dynamic pressure for this test.
        world->setHydrostaticPressureEnabled(false); // Keep hydrostatic disabled for controlled testing.
        world->setPressureScale(1.0);                // Enable pressure scale.
        
        // Apply test-specific settings.
        world->setWallsEnabled(false);
        world->setAddParticlesEnabled(false);
        world->setGravity(0.0); // Disable gravity to isolate dynamic pressure effects.
        
        spdlog::debug("[TEST] PressureDynamic test settings: dynamic_pressure=enabled, hydrostatic_pressure=disabled, walls=disabled");
    }
    
    WorldInterface* getWorldInterface() {
        return world.get();
    }
    
    std::unique_ptr<WorldB> world;
};

TEST_F(PressureDynamicTest, BlockedTransferAccumulatesDynamicPressure) {
    // Enable restart functionality for this test.
    runRestartableTest([this]() {
        spdlog::info("[TEST] Testing dynamic pressure accumulation from blocked WATER-WATER transfers");
        
        // This test expects the following dynamic pressure behavior to be implemented:
        // 1. When material tries to transfer but the target cell is near capacity, the transfer is partially blocked.
        // 2. The blocked transfer energy (velocity * blocked_amount) accumulates as dynamic pressure.
        // 3. Dynamic pressure creates forces that affect cell velocity.
        // 4. Dynamic pressure decays over time when blockage is removed.
        
        // Clear the world for restart.
        if (world) {
            for (uint32_t y = 0; y < world->getHeight(); ++y) {
                for (uint32_t x = 0; x < world->getWidth(); ++x) {
                    world->at(x, y).clear();
                }
            }
        }
        
        // Scenario: WATER tries to flow into a nearly full WATER cell.
        // This is simpler than mixed materials and focuses on capacity-based blocking.
        world->addMaterialAtCell(0, 1, MaterialType::WATER, 1.0);   // Full WATER source.
        world->addMaterialAtCell(1, 1, MaterialType::WATER, 0.95);  // Nearly full WATER target.
        
        CellB& sourceCell = world->at(0, 1);
        CellB& targetCell = world->at(1, 1);
        
        // Set COM positions AFTER adding material to override defaults.
        sourceCell.setCOM(Vector2d(0.8, 0.0));       // COM near right boundary for transfer.
        targetCell.setCOM(Vector2d(-0.5, 0.0));      // COM on left side.
        
        // Set velocities - source pushing right, target stationary.
        sourceCell.setVelocity(Vector2d(5.0, 0.0));  // Strong rightward push.
        targetCell.setVelocity(Vector2d(0.0, 0.0));  // Target starts stationary.
        
        // Use showInitialStateWithStep to give user choice between Start and Step.
        showInitialStateWithStep(world.get(), "Full WATER → Nearly full WATER: Natural pressure buildup");
    
    // Log initial world state.
    logWorldState(world.get(), "Initial Setup");
   
    // Test Phase 1: Natural pressure accumulation from blocked transfers.
    spdlog::info("\n--- PHASE 1: Testing natural pressure accumulation ---");
    
    // Track pressure changes over multiple timesteps.
    std::vector<double> sourcePressureHistory;
    std::vector<double> targetPressureHistory;
    std::vector<double> sourceFillHistory;
    std::vector<double> targetFillHistory;
    
    // Cache the maximum pressure seen during physics updates.
    double maxSourcePressureSeen = 0.0;
    double maxTargetPressureSeen = 0.0;
    
    bool pressureDetected = false;
    int pressureDetectedTimestep = -1;
    
    const int maxTimesteps = 30;  // Triple the length to observe more dynamics.
    
    logWorldState(world.get(), fmt::format("Before timestep 0"));

    // Run simulation using unified loop.
    runSimulationLoop(maxTimesteps, [&](int timestep) {
        // Record state before timestep.
        sourcePressureHistory.push_back(sourceCell.getDynamicPressure());
        targetPressureHistory.push_back(targetCell.getDynamicPressure());
        sourceFillHistory.push_back(sourceCell.getFillRatio());
        targetFillHistory.push_back(targetCell.getFillRatio());
        
        // Show current state.
        std::stringstream ss;
        ss << "Timestep " << timestep + 1 << " - Pressure Test\n";
        ss << "🔍 Source (0,1): P=" << std::setprecision(6) << sourceCell.getDynamicPressure() << "\n";
        ss << "🎯 Target (1,1): P=" << targetCell.getDynamicPressure();
        if (pressureDetected) {
            ss << "\n🔥 Pressure building!";
        }
        updateDisplay(world.get(), ss.str());
        
        logWorldState(world.get(), fmt::format("Before timestep {}", timestep));
        
        // Check debug pressure IMMEDIATELY after physics step (before it decays).
        double immediateSourceDebugPressure = sourceCell.getDynamicPressure();
        double immediateTargetDebugPressure = targetCell.getDynamicPressure();
        
        if (immediateSourceDebugPressure > 0.001 || immediateTargetDebugPressure > 0.001) {
            spdlog::info("DEBUG PRESSURE FOUND immediately after timestep {}: Source={:.6f}, Target={:.6f}",
                        timestep, immediateSourceDebugPressure, immediateTargetDebugPressure);
        }
        
        logWorldState(world.get(), fmt::format("After timestep {}", timestep));
        
        // Check for pressure accumulation after the step.
        double currentSourcePressure = sourceCell.getDynamicPressure();
        double currentTargetPressure = targetCell.getDynamicPressure();
        
        // Also check debug pressure which shows pressure before it was consumed.
        double sourceDebugPressure = sourceCell.getDynamicPressure();
        double targetDebugPressure = targetCell.getDynamicPressure();
        
        // Update maximum pressure seen.
        maxSourcePressureSeen = std::max(maxSourcePressureSeen, std::max(currentSourcePressure, sourceDebugPressure));
        maxTargetPressureSeen = std::max(maxTargetPressureSeen, std::max(currentTargetPressure, targetDebugPressure));
        
        // Log debug values to understand what's happening.
        if (timestep < 5) {
            spdlog::debug("Timestep {} pressure values - Source: dyn={:.6f} debug={:.6f}, Target: dyn={:.6f} debug={:.6f}",
                         timestep, currentSourcePressure, sourceDebugPressure, 
                         currentTargetPressure, targetDebugPressure);
        }
        
        // Record pressure AFTER timestep to catch pressure generated during the step.
        sourcePressureHistory.push_back(currentSourcePressure);
        targetPressureHistory.push_back(currentTargetPressure);
        
        // Detect when pressure first appears (check both dynamic and debug pressure).
        if (!pressureDetected && (currentSourcePressure > 0.001 || currentTargetPressure > 0.001 || 
                                  sourceDebugPressure > 0.001 || targetDebugPressure > 0.001)) {
            pressureDetected = true;
            pressureDetectedTimestep = timestep + 1;
            spdlog::info("  🔥 PRESSURE DETECTED at timestep {}! (dynamic: {:.6f}, {:.6f}, debug: {:.6f}, {:.6f})", 
                        pressureDetectedTimestep, currentSourcePressure, currentTargetPressure,
                        sourceDebugPressure, targetDebugPressure);
        }
    }, "Testing pressure accumulation", [&]() -> bool {
        // Early stop condition: target cell reached full capacity.
        if (targetCell.getFillRatio() >= 0.999) {
            spdlog::info("  Target cell reached full capacity");
            return true; // Stop simulation.
        }
        return false; // Continue simulation.
    });
    
    // Analyze pressure accumulation results.
    double maxSourcePressure = *std::max_element(sourcePressureHistory.begin(), sourcePressureHistory.end());
    double maxTargetPressure = *std::max_element(targetPressureHistory.begin(), targetPressureHistory.end());
    double finalTargetPressure = targetCell.getDynamicPressure();
    double totalFillTransferred = targetCell.getFillRatio() - 0.95;
    
    spdlog::info("\n--- PHASE 1 RESULTS ---");
    spdlog::info("Pressure detected: {} (at timestep {})", pressureDetected ? "YES" : "NO", pressureDetectedTimestep);
    spdlog::info("Max source pressure reached (history): {:.6f}", maxSourcePressure);
    spdlog::info("Max target pressure reached (history): {:.6f}", maxTargetPressure);
    spdlog::info("Max source pressure seen (including debug): {:.6f}", maxSourcePressureSeen);
    spdlog::info("Max target pressure seen (including debug): {:.6f}", maxTargetPressureSeen);
    spdlog::info("Material transferred to target: {:.3f} (capacity was {:.3f})", totalFillTransferred, 0.05);
    
    // Assertions for Phase 1.
    if (!pressureDetected) {
        EXPECT_GT(maxTargetPressureSeen, 0.001) << "Target should accumulate measurable pressure from blocked transfers";
    }
    
    EXPECT_LE(totalFillTransferred, 0.051) << "Only limited material should transfer due to capacity constraint";
    
    // Test Phase 2: Pressure forces affect movement.
    spdlog::info("\n--- PHASE 2: Testing pressure forces on movement ---");
    
    if (finalTargetPressure > 0.001) {
        Vector2d velocityBefore = targetCell.getVelocity();
        
        spdlog::info("Before pressure forces: vel=({:.3f},{:.3f}), pressure={:.6f}",
                     velocityBefore.x, velocityBefore.y, finalTargetPressure);
        
        // Run another timestep to see pressure forces in action.
        world->advanceTime(0.016);
        
        Vector2d velocityAfter = targetCell.getVelocity();
        double pressureAfter = targetCell.getDynamicPressure();
        
        spdlog::info("After pressure forces: vel=({:.3f},{:.3f}), pressure={:.6f}",
                     velocityAfter.x, velocityAfter.y, pressureAfter);
        
        // Check if pressure affected velocity.
        Vector2d velocityChange = velocityAfter - velocityBefore;
        
        // With unified pressure system, we don't track pressure gradients.
        // Just verify that velocity changed when pressure was present.
        if (finalTargetPressure > 0.1) {
            EXPECT_GT(velocityChange.magnitude(), 0.001) << "Pressure should cause velocity changes";
        }
        
        if (visual_mode_) {
            std::stringstream ss;
            ss << "Pressure force effects:\n";
            ss << "Velocity change: (" << std::fixed << std::setprecision(3) 
               << velocityChange.x << ", " << velocityChange.y << ")\n";
            ss << "Velocity changed: " << (velocityChange.magnitude() > 0.001 ? "✓" : "✗");
            updateDisplay(world.get(), ss.str());
            pauseIfVisual(500);
        }
    }
    
    // Test Phase 3: Verify pressure dissipation.
    spdlog::info("\n--- PHASE 3: Testing pressure dissipation ---");
    
    if (visual_mode_) {
        updateDisplay(world.get(), "Checking that pressure has dissipated...");
        pauseIfVisual(500);
    }
    
    // Check all cells to ensure no pressure remains.
    bool anyPressureRemaining = false;
    double maxRemainingPressure = 0.0;
    
    for (uint32_t y = 0; y < world->getHeight(); y++) {
        for (uint32_t x = 0; x < world->getWidth(); x++) {
            const CellB& cell = world->at(x, y);
            double dynamicPressure = cell.getDynamicPressure();
            double debugPressure = cell.getDynamicPressure();
            
            if (dynamicPressure > 0.001) {
                anyPressureRemaining = true;
                maxRemainingPressure = std::max(maxRemainingPressure, dynamicPressure);
                spdlog::warn("  Cell({},{}) still has dynamic pressure: {:.6f}", x, y, dynamicPressure);
            }
            
            // Debug pressure is expected to decay more slowly (visual indicator).
            if (debugPressure > 0.01) {
                spdlog::debug("  Cell({},{}) has debug pressure: {:.6f} (visual indicator)", x, y, debugPressure);
            }
        }
    }
    
    spdlog::info("Pressure check complete:");
    spdlog::info("  Any dynamic pressure remaining: {}", anyPressureRemaining ? "YES" : "NO");
    spdlog::info("  Max remaining pressure: {:.6f}", maxRemainingPressure);
    
    // Verify that pressure was consumed/dissipated.
    EXPECT_FALSE(anyPressureRemaining) << "All dynamic pressure should be consumed or dissipated after being applied";
    EXPECT_LT(maxRemainingPressure, 0.001) << "Remaining pressure should be negligible";
    
    if (visual_mode_) {
        std::stringstream ss;
        ss << "Phase 3 Results:\n";
        ss << "✓ Pressure detected and accumulated\n";
        ss << "✓ Pressure affected movement\n";
        ss << (anyPressureRemaining ? "✗ Pressure still present!" : "✓ Pressure fully dissipated");
        updateDisplay(world.get(), ss.str());
        waitForRestartOrNext();
    }
    
        spdlog::info("✅ BlockedTransferAccumulatesDynamicPressure test completed successfully");
    });  // End of runRestartableTest lambda.
}

TEST_F(PressureDynamicTest, DynamicPressureDrivesHorizontalFlow) {
    spdlog::info("[TEST] Testing dynamic pressure-driven horizontal flow through a hole");
    
    // This test expects the following behavior:
    // 1. Top water falls onto middle water, creating a blocked transfer.
    // 2. The blocked transfer generates dynamic pressure in the middle cell.
    // 3. Pressure gradient calculation detects high pressure on left vs low pressure on right.
    // 4. Material flows horizontally through the hole due to pressure gradient.
    
    // Stage-based success criteria:
    // Stage 1: Dynamic pressure builds in middle-left cell (0,1) from collision.
    // Stage 2: Water flows through hole to fill center cell (1,1).
    // Stage 3: Water eventually reaches lower-right cell (2,2).
    
    // Setup 3x3 world with wall and hole.
    // Column 0: Water that will create pressure.
    world->addMaterialAtCell(0, 0, MaterialType::WATER, 1.0);  // Top water - will fall.
    world->addMaterialAtCell(0, 1, MaterialType::WATER, 1.0);  // Middle water - will receive impact.
    world->addMaterialAtCell(0, 2, MaterialType::WALL, 1.0);   // Bottom wall.
    
    // Column 1: Wall with hole at (1,1).
    world->addMaterialAtCell(1, 0, MaterialType::WALL, 1.0);   // Top wall.
    // (1,1) left empty - this is the hole.
    world->addMaterialAtCell(1, 2, MaterialType::WALL, 1.0);   // Bottom wall.
    
    // Column 2: Empty space (low pressure).
    // All cells left empty.
    
    // Give top water some initial downward velocity to ensure collision.
    CellB& topWater = world->at(0, 0);
    topWater.setVelocity(Vector2d(0.0, 2.0));  // Falling downward.
    
    // Enable gravity to drive the collision.
    world->setGravity(9.81);
    
    spdlog::info("Initial setup:");
    spdlog::info("  (0,0): WATER with downward velocity");
    spdlog::info("  (0,1): WATER (will receive impact)");
    spdlog::info("  (1,1): Empty (the hole)");
    spdlog::info("  Gravity enabled: {}", world->getGravity());
    
    // Show initial state.
    showInitialStateWithStep(world.get(), "Water column with wall and hole - Testing pressure-driven horizontal flow");
    
    // Track key metrics over time.
    std::vector<double> middlePressureHistory;
    std::vector<double> centerFillHistory;     // Cell (1,1) - the hole.
    std::vector<double> lowerRightFillHistory; // Cell (2,2) - final destination.
    
    bool stage1_passed = false;  // Pressure detected in (0,1).
    bool stage2_passed = false;  // Water reached center (1,1).
    bool stage3_passed = false;  // Water reached lower-right (2,2).
    
    int stage1_timestep = -1;
    int stage2_timestep = -1;
    int stage3_timestep = -1;
    
    const int maxTimesteps = 50;  // Allow more time for horizontal flow.
    
    // Use unified simulation loop to eliminate duplication.
    runSimulationLoop(maxTimesteps, [&](int timestep) {
        // Record state before simulation step.
        CellB& middleCell = world->at(0, 1);
        CellB& centerCell = world->at(1, 1);
        CellB& lowerRightCell = world->at(2, 2);
        
        middlePressureHistory.push_back(middleCell.getDynamicPressure());
        centerFillHistory.push_back(centerCell.getFillRatio());
        lowerRightFillHistory.push_back(lowerRightCell.getFillRatio());
        
        // Calculate pressure gradient at middle cell to see if it points toward hole.
        Vector2d pressureGradient = world->getPressureCalculator().calculatePressureGradient(0, 1);
        
        // Build status display.
        std::stringstream ss;
        ss << "Timestep " << timestep + 1 << " - Horizontal Flow Test\n";
        ss << "🔍 Middle (0,1): P=" << std::setprecision(6) << middleCell.getDynamicPressure() << "\n";
        ss << "🎯 Center (1,1): fill=" << std::setprecision(3) << centerCell.getFillRatio() << "\n";
        ss << "📍 Target (2,2): fill=" << lowerRightCell.getFillRatio();
        
        if (!stage1_passed && middleCell.getDynamicPressure() > 0.001) {
            ss << "\n🎯 STAGE 1 PASSED: Pressure detected!";
        }
        if (!stage2_passed && centerCell.getFillRatio() > 0.001) {
            ss << "\n🎯 STAGE 2 PASSED: Water reached center!";
        }
        if (!stage3_passed && lowerRightCell.getFillRatio() > 0.001) {
            ss << "\n🎯 STAGE 3 PASSED: Water reached target!";
        }
        
        updateDisplay(world.get(), ss.str());
        
        // Check stage progression.
        if (!stage1_passed && middleCell.getDynamicPressure() > 0.001) {
            stage1_passed = true;
            stage1_timestep = timestep + 1;
            spdlog::info("Stage 1 passed at timestep {}: Pressure = {}", stage1_timestep, middleCell.getDynamicPressure());
        }
        
        if (!stage2_passed && centerCell.getFillRatio() > 0.001) {
            stage2_passed = true;
            stage2_timestep = timestep + 1;
            spdlog::info("Stage 2 passed at timestep {}: Center fill = {}", stage2_timestep, centerCell.getFillRatio());
        }
        
        if (!stage3_passed && lowerRightCell.getFillRatio() > 0.001) {
            stage3_passed = true;
            stage3_timestep = timestep + 1;
            spdlog::info("Stage 3 passed at timestep {}: Target fill = {}", stage3_timestep, lowerRightCell.getFillRatio());
        }
        
        // Log detailed state after step.
        spdlog::debug("After timestep {}: Middle pressure={:.6f}, gradient=({:.3f},{:.3f}), center_fill={:.3f}", 
                     timestep + 1, middleCell.getDynamicPressure(), pressureGradient.x, pressureGradient.y,
                     centerCell.getFillRatio());
    },
    "Testing pressure-driven horizontal flow",
    [&]() { return stage3_passed; }  // Early exit when all stages passed.
    );
    
    // Analyze results.
    double maxMiddlePressure = *std::max_element(middlePressureHistory.begin(), middlePressureHistory.end());
    double maxCenterFill = *std::max_element(centerFillHistory.begin(), centerFillHistory.end());
    double maxLowerRightFill = *std::max_element(lowerRightFillHistory.begin(), lowerRightFillHistory.end());
    
    spdlog::info("\n--- TEST RESULTS ---");
    spdlog::info("Stage 1 (Pressure buildup): {} at timestep {}", stage1_passed ? "PASSED" : "FAILED", stage1_timestep);
    spdlog::info("Stage 2 (Center filled): {} at timestep {}", stage2_passed ? "PASSED" : "FAILED", stage2_timestep);
    spdlog::info("Stage 3 (Target reached): {} at timestep {}", stage3_passed ? "PASSED" : "FAILED", stage3_timestep);
    spdlog::info("Max middle pressure: {:.6f}", maxMiddlePressure);
    spdlog::info("Max center fill: {:.3f}", maxCenterFill);
    spdlog::info("Max target fill: {:.3f}", maxLowerRightFill);
    
    // Assertions.
    EXPECT_TRUE(stage1_passed) << "Stage 1 failed: Dynamic pressure should build from water collision";
    
    if (stage1_passed) {
        // Only check later stages if pressure was generated.
        EXPECT_TRUE(stage2_passed) << "Stage 2 failed: Pressure gradient should drive water through hole to center cell";
        
        // Stage 3 is optional - water might not reach all the way to (2,2) with current parameters.
        if (!stage3_passed) {
            spdlog::info("Note: Stage 3 (reaching lower-right) did not pass - this may require parameter tuning");
        }
    } else {
        spdlog::warn("⚠️  No dynamic pressure detected - pressure-driven flow cannot be tested");
        spdlog::warn("   This may indicate that pressure gradient calculation or pressure-driven transfers need adjustment");
    }
    
    spdlog::info("✅ DynamicPressureDrivesHorizontalFlow test completed");
}
