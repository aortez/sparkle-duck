#include "visual_test_runner.h"
#include "../WorldB.h"
#include "../MaterialType.h"
#include "../WorldCohesionCalculator.h"
#include "spdlog/spdlog.h"
#include <thread>
#include <chrono>

class HorizontalLineStabilityTest : public VisualTestBase {
protected:
    void SetUp() override {
        VisualTestBase::SetUp();
        
        // Apply auto-scaling for 4x2 world before creation
        if (visual_mode_ && auto_scaling_enabled_) {
            scaleDrawingAreaForWorld(4, 2);
        }
        
        // Create a small 4x2 world for testing horizontal line stability
        // Pass the UI draw area if in visual mode, otherwise nullptr
        lv_obj_t* draw_area = (visual_mode_ && ui_) ? ui_->getDrawArea() : nullptr;
        world = std::make_unique<WorldB>(4, 2, draw_area);
        
        // Disable walls to prevent boundary interference with our test setup
        world->setWallsEnabled(false);
        
        // Set up the test scenario:
        // MDD-  (top row: Metal at (0,0), Dirt at (1,0) and (2,0), empty at (3,0))
        // ----  (bottom row: all empty)
        
        world->addMaterialAtCell(0, 0, MaterialType::METAL, 1.0);  // Support anchor
        world->addMaterialAtCell(1, 0, MaterialType::DIRT, 1.0);   // Connected dirt
        world->addMaterialAtCell(2, 0, MaterialType::DIRT, 1.0);   // Cantilever dirt (should fall)
        
        spdlog::info("=== Test Setup Complete ===");
        spdlog::info("Initial configuration:");
        spdlog::info("(0,0): METAL  (1,0): DIRT  (2,0): DIRT  (3,0): EMPTY");
        spdlog::info("(0,1): EMPTY  (1,1): EMPTY  (2,1): EMPTY  (3,1): EMPTY");
        
        // Log initial state details
        logCellDetails(0, 0, "METAL anchor");
        logCellDetails(1, 0, "DIRT connected");
        logCellDetails(2, 0, "DIRT cantilever (should fall)");
    }
    
    void TearDown() override {
        world.reset();
        VisualTestBase::TearDown();
    }
    
    void logCellDetails(uint32_t x, uint32_t y, const std::string& description) {
        const CellB& cell = world->at(x, y);
        auto cohesion = WorldCohesionCalculator(*world).calculateCohesionForce(x, y);
        auto adhesion = world->calculateAdhesionForce(x, y);
        
        spdlog::info("Cell ({},{}) - {}: material={}, fill={:.1f}, neighbors={}, cohesion_resistance={:.3f}, adhesion_magnitude={:.3f}",
                     x, y, description, 
                     getMaterialName(cell.getMaterialType()), 
                     cell.getFillRatio(),
                     cohesion.connected_neighbors,
                     cohesion.resistance_magnitude,
                     adhesion.force_magnitude);
    }
    
    void logForceAnalysis(uint32_t x, uint32_t y, double deltaTime = 0.016) {
        const CellB& cell = world->at(x, y);
        auto cohesion = WorldCohesionCalculator(*world).calculateCohesionForce(x, y);
        auto adhesion = world->calculateAdhesionForce(x, y);
        
        // Calculate forces as done in queueMaterialMoves
        Vector2d gravity_force(0.0, 9.81 * deltaTime * getMaterialDensity(cell.getMaterialType()));
        Vector2d net_driving_force = gravity_force + adhesion.force_direction * adhesion.force_magnitude;
        double driving_magnitude = net_driving_force.mag();
        double movement_threshold = cohesion.resistance_magnitude;
        
        bool will_move = driving_magnitude > movement_threshold;
        
        spdlog::info("Force analysis for ({},{}):", x, y);
        spdlog::info("  Gravity force: ({:.3f}, {:.3f}) magnitude: {:.3f}", 
                     gravity_force.x, gravity_force.y, gravity_force.mag());
        spdlog::info("  Adhesion force: ({:.3f}, {:.3f}) magnitude: {:.3f}",
                     adhesion.force_direction.x, adhesion.force_direction.y, adhesion.force_magnitude);
        spdlog::info("  Net driving force: ({:.3f}, {:.3f}) magnitude: {:.3f}",
                     net_driving_force.x, net_driving_force.y, driving_magnitude);
        spdlog::info("  Cohesion resistance: {:.3f}", movement_threshold);
        spdlog::info("  Will move: {} (driving {:.3f} {} resistance {:.3f})",
                     will_move ? "YES" : "NO", driving_magnitude, 
                     will_move ? ">" : "<=", movement_threshold);
    }
    
    bool isDirtAtPosition(uint32_t x, uint32_t y) {
        if (x >= world->getWidth() || y >= world->getHeight()) return false;
        const CellB& cell = world->at(x, y);
        return cell.getMaterialType() == MaterialType::DIRT && cell.getFillRatio() > 0.1;
    }
    
    void updateVisualDisplay() {
        if (visual_mode_ && world) {
            auto& coordinator = VisualTestCoordinator::getInstance();
            coordinator.postTaskSync([this] {
                world->draw();
            });
        }
    }
    
    std::unique_ptr<WorldB> world;
};

TEST_F(HorizontalLineStabilityTest, CantileverDirtShouldFall) {
    // Initial state verification
    EXPECT_TRUE(isDirtAtPosition(2, 0)) << "Cantilever dirt should be at (2,0) initially";
    EXPECT_FALSE(isDirtAtPosition(2, 1)) << "Position (2,1) should be empty initially";
    
    spdlog::info("=== Initial Force Analysis ===");
    logForceAnalysis(2, 0);  // Cantilever dirt
    logForceAnalysis(1, 0);  // Connected dirt
    
    // Show initial state in visual mode and wait for user to start
    updateVisualDisplay();
    waitForStart(); // Wait for user to press Start button
    
    // Pause for 1 second after showing the first frame
    if (visual_mode_) {
        spdlog::info("Pausing for 1 second to observe initial state...");
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    
    // Run simulation for several timesteps to see if cantilever dirt falls
    const double deltaTime = 0.016;  // ~60fps
    const int maxSteps = 100;
    
    bool cantileverFell = false;
    int stepWhenFell = -1;
    
    for (int step = 0; step < maxSteps; step++) {
        spdlog::info("=== Simulation Step {} ===", step + 1);
        
        // Log forces before movement
        if (step < 5 || step % 10 == 0) {  // Log first 5 steps, then every 10th
            logForceAnalysis(2, 0);
        }
        
        // Clear pending moves and queue new ones
        world->clearPendingMoves();
        world->queueMaterialMovesForTesting(deltaTime);
        
        // Check if cantilever dirt has a pending move
        const auto& pendingMoves = world->getPendingMoves();
        bool cantileverHasMove = false;
        for (const auto& move : pendingMoves) {
            if (move.fromX == 2 && move.fromY == 0) {
                cantileverHasMove = true;
                spdlog::info("Cantilever dirt has pending move: ({},{}) -> ({},{}) amount={:.3f}",
                           move.fromX, move.fromY, move.toX, move.toY, move.amount);
                break;
            }
        }
        
        if (!cantileverHasMove && step < 5) {
            spdlog::info("Cantilever dirt has NO pending moves in step {}", step + 1);
        }
        
        // Advance the world one timestep
        world->advanceTime(deltaTime);
        
        // Update visual display every step.
		updateVisualDisplay();
        
        // Check if cantilever dirt has fallen
        if (!isDirtAtPosition(2, 0)) {
            cantileverFell = true;
            stepWhenFell = step + 1;
            spdlog::info("Cantilever dirt fell at step {}!", stepWhenFell);
            
            // Pause for 1 second before the final frame to observe the fall
            if (visual_mode_) {
                spdlog::info("Pausing for 1 second to observe final state...");
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
            break;
        }
        
        // Also check if it moved down to (2,1)
        if (isDirtAtPosition(2, 1)) {
            cantileverFell = true;
            stepWhenFell = step + 1;
            spdlog::info("Cantilever dirt moved to (2,1) at step {}!", stepWhenFell);
            
            // Pause for 1 second before the final frame to observe the fall
            if (visual_mode_) {
                spdlog::info("Pausing for 1 second to observe final state...");
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
            break;
        }
    }
    
    spdlog::info("=== Final State Analysis ===");
    spdlog::info("Cantilever fell: {} (step: {})", cantileverFell, stepWhenFell);
    spdlog::info("Final positions:");
    for (uint32_t y = 0; y < 2; y++) {
        for (uint32_t x = 0; x < 4; x++) {
            const CellB& cell = world->at(x, y);
            if (!cell.isEmpty()) {
                spdlog::info("  ({},{}): {} fill={:.1f} velocity=({:.3f},{:.3f})",
                           x, y, getMaterialName(cell.getMaterialType()), 
                           cell.getFillRatio(), cell.getVelocity().x, cell.getVelocity().y);
            }
        }
    }
    
    // Wait for user to observe final state in visual mode
    waitForNext();
    
    // The test expectation: cantilever dirt should fall
    // This will currently FAIL, demonstrating the horizontal line stability problem
    EXPECT_TRUE(cantileverFell) 
        << "Cantilever dirt should fall due to gravity, but cohesion from 1 neighbor (resistance=0.4) "
        << "is stronger than gravity force (~0.24), creating an unrealistic floating bridge effect";
        
    if (cantileverFell) {
        spdlog::info("SUCCESS: Cantilever dirt fell as expected (realistic physics)");
    } else {
        spdlog::error("PROBLEM: Cantilever dirt stayed suspended (unrealistic infinite bridge)");
        spdlog::error("This demonstrates the horizontal line stability problem in the cohesion system");
    }
}

TEST_F(HorizontalLineStabilityTest, ConnectedDirtShouldStayStable) {
    // The dirt at (1,0) should stay stable since it's connected to metal support
    // and has proper structural backing
    
    spdlog::info("=== Testing Connected Dirt Stability ===");
    logForceAnalysis(1, 0);  // Connected dirt
    
    // Wait for user to start this test phase
    waitForStart();
    
    const double deltaTime = 0.016;
    const int testSteps = 50;
    
    for (int step = 0; step < testSteps; step++) {
        world->advanceTime(deltaTime);
        updateVisualDisplay(); // Show progress during simulation
        
        // Connected dirt should remain stable
        EXPECT_TRUE(isDirtAtPosition(1, 0)) 
            << "Connected dirt should remain stable at step " << (step + 1);
    }
    
    spdlog::info("Connected dirt remained stable as expected (good structural support)");
    
    // Wait for user to observe the stable result
    waitForNext();
}

TEST_F(HorizontalLineStabilityTest, FloatingLShapeShouldCollapse) {
    // Test a floating L-shaped structure with no structural support
    // ----
    // DDD-
    // D---
    // ----
    // L-structure is floating in middle, away from ground support
    
    // Resize world to 4x4 to have floating structure away from ground
    world->resizeGrid(4, 4);
    
    // Clear all cells
    for (uint32_t y = 0; y < 4; y++) {
        for (uint32_t x = 0; x < 4; x++) {
            world->at(x, y).clear();
        }
    }
    
    // Set up L-shaped floating structure in middle of world (away from ground at y=3)
    world->addMaterialAtCell(0, 1, MaterialType::DIRT, 1.0);  // Corner
    world->addMaterialAtCell(1, 1, MaterialType::DIRT, 1.0);  // Horizontal arm
    world->addMaterialAtCell(2, 1, MaterialType::DIRT, 1.0);  // End of horizontal arm
    world->addMaterialAtCell(0, 2, MaterialType::DIRT, 1.0);  // Vertical arm
    
    spdlog::info("=== L-Shape Collapse Test Setup ===");
    spdlog::info("Initial configuration (4x4 world):");
    spdlog::info("----  (row 0: all empty)");
    spdlog::info("DDD-  (row 1: Dirt at (0,1), (1,1), (2,1), empty at (3,1))");
    spdlog::info("D---  (row 2: Dirt at (0,2), empty elsewhere)");
    spdlog::info("----  (row 3: all empty - this is the ground)");
    
    // Log initial state of all dirt cells
    logCellDetails(0, 1, "L-corner");
    logCellDetails(1, 1, "horizontal-arm");
    logCellDetails(2, 1, "horizontal-end");
    logCellDetails(0, 2, "vertical-arm");
    
    // Count initial dirt cells
    int initialDirtCount = 0;
    for (uint32_t y = 0; y < 4; y++) {
        for (uint32_t x = 0; x < 4; x++) {
            if (isDirtAtPosition(x, y)) {
                initialDirtCount++;
            }
        }
    }
    
    spdlog::info("Initial dirt count: {}", initialDirtCount);
    EXPECT_EQ(initialDirtCount, 4) << "Should start with 4 dirt cells";
    
    spdlog::info("=== Initial Force Analysis ===");
    logForceAnalysis(0, 1);  // L-corner
    logForceAnalysis(1, 1);  // horizontal-arm  
    logForceAnalysis(2, 1);  // horizontal-end
    logForceAnalysis(0, 2);  // vertical-arm
    
    // Run simulation to see if floating structure collapses
    const double deltaTime = 0.016;
    const int maxSteps = 100;
    
    bool structureCollapsed = false;
    int stepWhenCollapsed = -1;
    int topRowDirtCount = 0;
    
    for (int step = 0; step < maxSteps; step++) {
        spdlog::info("=== Simulation Step {} ===", step + 1);
        
        // Count dirt cells in floating rows before movement (rows 1 and 2)
        topRowDirtCount = 0;
        for (uint32_t x = 0; x < 4; x++) {
            if (isDirtAtPosition(x, 1) || isDirtAtPosition(x, 2)) {
                topRowDirtCount++;
            }
        }
        
        // Log forces for first few steps
        if (step < 3) {
            logForceAnalysis(0, 1);  // L-corner (if still there)
            if (isDirtAtPosition(2, 1)) {
                logForceAnalysis(2, 1);  // horizontal-end
            }
        }
        
        // Advance simulation
        world->advanceTime(deltaTime);
        
        // Check if structure has started collapsing (dirt moved from floating rows)
        int newTopRowDirtCount = 0;
        for (uint32_t x = 0; x < 4; x++) {
            if (isDirtAtPosition(x, 1) || isDirtAtPosition(x, 2)) {
                newTopRowDirtCount++;
            }
        }
        
        if (newTopRowDirtCount < topRowDirtCount) {
            structureCollapsed = true;
            stepWhenCollapsed = step + 1;
            spdlog::info("Structure started collapsing at step {}! Top row dirt: {} -> {}", 
                        stepWhenCollapsed, topRowDirtCount, newTopRowDirtCount);
            break;
        }
        
        // Also check if any dirt has moved to ground level (row 3)
        if (isDirtAtPosition(0, 3) || isDirtAtPosition(1, 3) || isDirtAtPosition(2, 3) || isDirtAtPosition(3, 3)) {
            structureCollapsed = true;
            stepWhenCollapsed = step + 1;
            spdlog::info("Dirt fell to ground level at step {}!", stepWhenCollapsed);
            break;
        }
    }
    
    spdlog::info("=== Final State Analysis ===");
    spdlog::info("Structure collapsed: {} (step: {})", structureCollapsed, stepWhenCollapsed);
    
    // Log final positions
    spdlog::info("Final positions:");
    for (uint32_t y = 0; y < 4; y++) {
        for (uint32_t x = 0; x < 4; x++) {
            const CellB& cell = world->at(x, y);
            if (!cell.isEmpty()) {
                spdlog::info("  ({},{}): {} fill={:.1f} velocity=({:.3f},{:.3f})",
                           x, y, getMaterialName(cell.getMaterialType()), 
                           cell.getFillRatio(), cell.getVelocity().x, cell.getVelocity().y);
            }
        }
    }
    
    // The test expectation: floating L-structure should collapse
    EXPECT_TRUE(structureCollapsed) 
        << "Floating L-shaped structure should collapse since it has no structural support. "
        << "Distance-based cohesion decay should reduce all cohesion to minimum (0.04), "
        << "allowing gravity (0.235) to overcome cohesion and cause collapse.";
        
    if (structureCollapsed) {
        spdlog::info("SUCCESS: Floating L-structure collapsed as expected (realistic physics)");
    } else {
        spdlog::error("PROBLEM: Floating L-structure remained suspended (unrealistic floating island)");
        spdlog::error("This suggests distance-based cohesion decay may not be working for disconnected structures");
    }
}
