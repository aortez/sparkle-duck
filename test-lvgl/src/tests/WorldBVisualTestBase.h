#pragma once

#include "visual_test_runner.h"
#include "../WorldB.h"
#include "../MaterialType.h"
#include <memory>
#include <spdlog/spdlog.h>

/**
 * Base class for WorldB visual tests with common setup behavior.
 * 
 * All WorldB tests that inherit from this class will automatically get:
 * - setAddParticlesEnabled(false) by default
 * - setWallsEnabled(false) for clean testing
 * - Default 3x3 world size (can be overridden)
 * - Automatic world setup with initial materials
 */
class WorldBVisualTestBase : public VisualTestBase {
protected:
    void SetUp() override {
        VisualTestBase::SetUp();
        
        // Default to 3x3 world, but tests can override these
        width = 3;
        height = 3;
        createTestWorldB();
    }
    
    void createTestWorldB() {
        // Create world using enhanced method that applies universal defaults
        world = createWorldB(width, height);
        
        // Apply WorldB-specific test defaults (in addition to universal defaults)
        world->setAddParticlesEnabled(false);  // Disable particle addition for tests
        world->setWallsEnabled(false);         // Disable walls for clean mass calculations
        world->setup();                        // Setup with initial materials (most tests want this)
        
        // Log that WorldB test-specific defaults were applied
        spdlog::debug("[TEST] WorldB-specific test defaults applied: addParticles=false, walls=false");
    }

    void TearDown() override {
        world.reset();
        VisualTestBase::TearDown();
    }
    
    // Test data members available to all WorldB tests
    std::unique_ptr<WorldB> world;
    uint32_t width;
    uint32_t height;
};