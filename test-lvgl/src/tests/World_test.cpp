#include "../World.h"

#include <gtest/gtest.h>
#include <memory>
#include <cmath>
#include <iostream>
#include <execinfo.h>
#include <unistd.h>

class WorldTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Default to 1x2 world, but tests can override these
        width = 1;
        height = 2;
        createWorld();
    }

    void createWorld() {
        world = std::make_unique<World>(width, height, nullptr);
        world->setAddParticlesEnabled(false);
    }

    void TearDown() override {
    }

    std::unique_ptr<World> world;
    uint32_t width;
    uint32_t height;
};

TEST_F(WorldTest, EmptyWorldAdvance) {

    world->advanceTime(16);
}

TEST_F(WorldTest, DirtTransferVerticalWithMomentum)
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
    Vector2d prevSourceCom = initialCom;

    // Advance time by enough frames for transfer to occur.
    for (int i = 0; i < 400; ++i) {
        world->advanceTime(16); 

        // Check invariants at each step
        EXPECT_LE(world->at(0, 0).dirty, prevSourceDirt);     // Source cell should always lose dirt.
        EXPECT_GE(world->at(0, 1).dirty, prevTargetDirt);     // Target cell should always gain dirt.
        EXPECT_LE(world->at(0, 0).dirty + world->at(0, 1).dirty, initialDirt + 0.0001); // Conservation of mass (with small epsilon).
        EXPECT_GE(world->at(0, 0).dirty + world->at(0, 1).dirty, initialDirt - 0.0001); // Conservation of mass (with small epsilon).

        if (world->at(0, 0).dirty > 0.0) {
            // If source cell has dirt, its COM should be moving down as dirt falls.
            EXPECT_GE(world->at(0, 0).com.y, prevSourceCom.y);
        }

        // Update previous values for next iteration.
        prevSourceDirt = world->at(0, 0).dirty;
        prevTargetDirt = world->at(0, 1).dirty;
        prevSourceCom = world->at(0, 0).com;
    }
}

// TEST_F(WorldTest, ReplicateMainSetup) {
//     // Create a 20x20 world like in main.cpp.
//     world = std::make_unique<World>(20, 20, nullptr);
//     world->reset();

//     // Run simulation for many frames to try to trigger errors.
//     for (int i = 0; i < 200; ++i) {
//         try {
//             world->advanceTime(16); // 16ms per frame (roughly 60 FPS)
            
//             // Calculate total mass
//             double totalMass = 0.0;
//             bool foundNaN = false;
//             for (int x = 0; x < 20; x++) {
//                 for (int y = 0; y < 20; y++) {
//                     double cellMass = world->at(x, y).dirty;
//                     if (std::isnan(cellMass)) {
//                         if (!foundNaN) {
//                             std::cout << "\nFound NaN at frame " << i << " in cell (" << x << "," << y << "):" << std::endl;
//                             std::cout << "Cell state: mass=" << cellMass 
//                                     << ", com=(" << world->at(x, y).com.x << "," << world->at(x, y).com.y << ")"
//                                     << ", v=(" << world->at(x, y).v.x << "," << world->at(x, y).v.y << ")" << std::endl;
//                             foundNaN = true;
//                         }
//                     }
//                     totalMass += cellMass;
//                 }
//             }
            
//         } catch (const std::runtime_error& e) {
//             std::cout << "Error at frame " << i << ": " << e.what() << std::endl;
//             throw;
//         }
//     }
// }

TEST_F(WorldTest, DirtTransferHorizontalWithMomentum) {
    // Create a 2x1 world (horizontal)
    width = 2;
    height = 1;
    createWorld();
    world->setGravity(0.0); // Disable gravity for this test
    
    // Place all dirt in the left cell, with rightward velocity
    world->at(0, 0).dirty = 1.0;
    world->at(0, 0).com = Vector2d(0.0, 0.0);
    world->at(0, 0).v = Vector2d(1.0, 0.0); // Rightward
    world->at(1, 0).dirty = 0.0;
    world->at(1, 0).com = Vector2d(0.0, 0.0);
    world->at(1, 0).v = Vector2d(0.0, 0.0);

    double prevLeft = world->at(0, 0).dirty;
    double prevRight = world->at(1, 0).dirty;
    double initialTotal = prevLeft + prevRight;

    for (int i = 0; i < 100; ++i) {
        world->advanceTime(16); // 16ms per frame
        double left = world->at(0, 0).dirty;
        double right = world->at(1, 0).dirty;
        std::cout << "Step " << i << ": left=" << left << ", right=" << right << std::endl;
        // Dirt should move from left to right
        EXPECT_LE(left, prevLeft);
        EXPECT_GE(right, prevRight);
        // Mass should be conserved
        EXPECT_NEAR(left + right, initialTotal, 1e-4);
        prevLeft = left;
        prevRight = right;
        // Dirt should not fall at all, the Y component of the COM should remain centered.
        EXPECT_NEAR(world->at(0, 0).com.y, 0.0, 1e-4);
        EXPECT_NEAR(world->at(1, 0).com.y, 0.0, 1e-4);
    }
    // At the end, most dirt should be in the right cell
    EXPECT_LT(world->at(0, 0).dirty, 0.5);
    EXPECT_GT(world->at(1, 0).dirty, 0.5);
}

TEST_F(WorldTest, GravityFreeDiagonalMovement) {
    // Create a 2x2 world
    width = 2;
    height = 2;
    createWorld();
    world->setGravity(0.0); // Disable gravity for this test
    
    // Place all dirt in the top-left cell with diagonal velocity
    world->at(0, 0).dirty = 1.0;
    world->at(0, 0).com = Vector2d(0.0, 0.0);
    world->at(0, 0).v = Vector2d(1.0, 1.0); // Diagonal movement

    double prevTopLeft = world->at(0, 0).dirty;
    double prevTopRight = world->at(1, 0).dirty;
    double prevBottomLeft = world->at(0, 1).dirty;
    double prevBottomRight = world->at(1, 1).dirty;
    double initialTotal = prevTopLeft + prevTopRight + prevBottomLeft + prevBottomRight;
    Vector2d prevCom = world->at(0, 0).com;

    // Store initial velocity
    Vector2d initialVelocity = world->at(0, 0).v;

    for (int i = 0; i < 100; ++i) {
        world->advanceTime(16); // 16ms per frame
        
        // Get current values
        double topLeft = world->at(0, 0).dirty;
        double topRight = world->at(1, 0).dirty;
        double bottomLeft = world->at(0, 1).dirty;
        double bottomRight = world->at(1, 1).dirty;
        Vector2d currentCom = world->at(0, 0).com;

        // Check mass conservation
        double totalMass = topLeft + topRight + bottomLeft + bottomRight;
        EXPECT_NEAR(totalMass, initialTotal, 1e-4);

        // Check velocity conservation - the velocity of any cell with mass should match initial velocity
        if (topLeft > 0.0) {
            EXPECT_NEAR(world->at(0, 0).v.x, initialVelocity.x, 1e-4);
            EXPECT_NEAR(world->at(0, 0).v.y, initialVelocity.y, 1e-4);
        }
        if (topRight > 0.0) {
            EXPECT_NEAR(world->at(1, 0).v.x, initialVelocity.x, 1e-4);
            EXPECT_NEAR(world->at(1, 0).v.y, initialVelocity.y, 1e-4);
        }
        if (bottomLeft > 0.0) {
            EXPECT_NEAR(world->at(0, 1).v.x, initialVelocity.x, 1e-4);
            EXPECT_NEAR(world->at(0, 1).v.y, initialVelocity.y, 1e-4);
        }
        if (bottomRight > 0.0) {
            EXPECT_NEAR(world->at(1, 1).v.x, initialVelocity.x, 1e-4);
            EXPECT_NEAR(world->at(1, 1).v.y, initialVelocity.y, 1e-4);
        }

        // Check COM movement
        if (topLeft > 0.0) {
            // If dirt is still in top-left cell, COM should be moving diagonally
            EXPECT_GT(currentCom.x, prevCom.x);
            EXPECT_GT(currentCom.y, prevCom.y);
        }

        // Check transfer to bottom-right cell
        if (currentCom.x > 1.0 && currentCom.y > 1.0) {
            // Once COM crosses threshold, dirt should start moving to bottom-right
            EXPECT_GT(bottomRight, prevBottomRight);
        }

        // Update previous values
        prevTopLeft = topLeft;
        prevTopRight = topRight;
        prevBottomLeft = bottomLeft;
        prevBottomRight = bottomRight;
        prevCom = currentCom;
    }

    // At the end, the dirt should be in the bottom-right cell.
    EXPECT_NEAR(world->at(1, 1).dirty, 1.0, 0.0001);
    EXPECT_NEAR(world->at(0, 0).dirty, 0.0, 0.0001);
    EXPECT_NEAR(world->at(0, 1).dirty, 0.0, 0.0001);
    EXPECT_NEAR(world->at(1, 0).dirty, 0.0, 0.0001);
}
