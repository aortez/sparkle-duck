#include "../World.h"

#include <gtest/gtest.h>
#include <memory>
#include <cmath>
#include <iostream>

class WorldTest : public ::testing::Test {
protected:
    void SetUp() {
        world = std::make_unique<World>(1, 2, nullptr);
        world->setAddParticlesEnabled(false);
    }

    void TearDown() {
    }

    std::unique_ptr<World> world;
};

TEST_F(WorldTest, DirtMovesDownward) {
    // Fill top cell with dirt.
    world->at(0, 0).dirty = 1.0;
    world->at(0, 0).com = Vector2d(0, -0.5); // Center of mass near top.
    world->at(0, 0).v = Vector2d(0, 0.1);    // Initial downward velocity.

    // Leave bottom cell empty.
    world->at(0, 1).dirty = 0.0;
    world->at(0, 1).com = Vector2d(0, 0);
    world->at(0, 1).v = Vector2d(0, 0);

    // Run simulation for 1 cycle.
    world->advanceTime(16); // 16ms per frame (roughly 60 FPS).

    // Check that some dirt has transferred.

}

TEST_F(WorldTest, DirtTransferWithMomentum)
{
    // Fill the top cell with dirt and give it some velocity.
    world->at(0, 0).dirty = 1.0;
    world->at(0, 0).com = Vector2d(0.0, 0.0);  // COM starts in center.
    world->at(0, 0).v = Vector2d(0.0, 1.0);    // Moving downward.

    // Store initial values for comparison.
    double initialDirt = world->at(0, 0).dirty;
    Vector2d initialCom = world->at(0, 0).com;
    Vector2d initialVel = world->at(0, 0).v;

    // Track previous step's values.
    double prevSourceDirt = initialDirt;
    double prevTargetDirt = 0.0;

    // Advance time by enough frames for transfer to occur.
    for (int i = 0; i < 24; ++i) {
        world->advanceTime(16); // 16ms per frame (roughly 60 FPS).

        // Check invariants at each step
        EXPECT_LE(world->at(0, 0).dirty, prevSourceDirt);     // Source cell should always lose dirt.
        EXPECT_GE(world->at(0, 1).dirty, prevTargetDirt);     // Target cell should always gain dirt.
        EXPECT_LE(world->at(0, 0).dirty + world->at(0, 1).dirty, initialDirt + 0.0001); // Conservation of mass (with small epsilon).
        EXPECT_GE(world->at(0, 0).dirty + world->at(0, 1).dirty, initialDirt - 0.0001); // Conservation of mass (with small epsilon).

        // Check COM and velocity invariants.
        if (world->at(0, 1).dirty > 0.0) {
            // If target cell has dirt, its COM should be negative (dirt near top).
            EXPECT_LT(world->at(0, 1).com.y, 0.0);
        }
        if (world->at(0, 0).dirty > 0.0) {
            // If source cell still has dirt, its COM should be moving up as dirt leaves.
            EXPECT_GE(world->at(0, 0).com.y, initialCom.y);
        }

        // Update previous values for next iteration.
        prevSourceDirt = world->at(0, 0).dirty;
        prevTargetDirt = world->at(0, 1).dirty;
    }
}

TEST_F(WorldTest, ReplicateMainSetup) {
    // Create a 20x20 world like in main.cpp.
    world = std::make_unique<World>(20, 20, nullptr);
    world->reset();

    // Run simulation for many frames to try to trigger errors.
    for (int i = 0; i < 200; ++i) {
        try {
            world->advanceTime(16); // 16ms per frame (roughly 60 FPS)
            
            // Calculate total mass
            double totalMass = 0.0;
            bool foundNaN = false;
            for (int x = 0; x < 20; x++) {
                for (int y = 0; y < 20; y++) {
                    double cellMass = world->at(x, y).dirty;
                    if (std::isnan(cellMass)) {
                        if (!foundNaN) {
                            std::cout << "\nFound NaN at frame " << i << " in cell (" << x << "," << y << "):" << std::endl;
                            std::cout << "Cell state: mass=" << cellMass 
                                    << ", com=(" << world->at(x, y).com.x << "," << world->at(x, y).com.y << ")"
                                    << ", v=(" << world->at(x, y).v.x << "," << world->at(x, y).v.y << ")" << std::endl;
                            foundNaN = true;
                        }
                    }
                    totalMass += cellMass;
                }
            }
            
        } catch (const std::runtime_error& e) {
            std::cout << "Error at frame " << i << ": " << e.what() << std::endl;
            throw;
        }
    }
} 
