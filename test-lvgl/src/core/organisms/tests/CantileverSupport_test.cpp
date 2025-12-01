#include "CellTrackerUtil.h"
#include "core/World.h"
#include "core/WorldDiagramGeneratorEmoji.h"
#include <gtest/gtest.h>
#include <iomanip>
#include <spdlog/spdlog.h>

using namespace DirtSim;

/**
 * Simple test for cantilever support mechanics.
 *
 * 3x3 world with wood in an L shape:
 *   W W W  (horizontal beam - row 0)
 *   W - -  (vertical post - row 1)
 *   W - -  (grounded - row 2)
 */
TEST(CantileverSupportTest, HorizontalBeamStaysSupported)
{
    spdlog::set_level(spdlog::level::info);

    // Create 3x3 world.
    auto world = std::make_unique<World>(3, 3);

    // Clear to air.
    for (uint32_t y = 0; y < 3; ++y) {
        for (uint32_t x = 0; x < 3; ++x) {
            world->getData().at(x, y).replaceMaterial(MaterialType::AIR, 0.0);
        }
    }

    // Create L-shaped wood structure (beam one row lower).
    world->getData().at(0, 2).replaceMaterial(MaterialType::WOOD, 1.0); // Bottom (grounded on edge)
    world->getData().at(0, 1).replaceMaterial(MaterialType::WOOD, 1.0); // Top of column (corner)
    world->getData().at(1, 1).replaceMaterial(MaterialType::WOOD, 1.0); // Beam middle (cantilever!)
    world->getData().at(2, 1).replaceMaterial(MaterialType::WOOD, 1.0); // Beam right (cantilever!)

    std::cout << "\n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n";
    std::cout << "CANTILEVER SUPPORT TEST\n";
    std::cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n";
    std::cout << "\nInitial structure:\n";
    std::cout << WorldDiagramGeneratorEmoji::generateEmojiDiagram(*world) << "\n";

    std::cout << "Expected support propagation:\n";
    std::cout << "  (0,2): vertical support (bottom edge)\n";
    std::cout << "  (0,1): support from (0,2) below\n";
    std::cout << "  (1,1): support from (0,1) to left  ← CANTILEVER!\n";
    std::cout << "  (2,1): support from (1,1) to left  ← CANTILEVER!\n\n";

    // Create tracker (organism_id=0 since these aren't tree cells).
    CellTracker tracker(*world, 0, 20);

    // Track all wood cells.
    tracker.trackCell(Vector2i{ 0, 2 }, MaterialType::WOOD, 0);
    tracker.trackCell(Vector2i{ 0, 1 }, MaterialType::WOOD, 0);
    tracker.trackCell(Vector2i{ 1, 1 }, MaterialType::WOOD, 0);
    tracker.trackCell(Vector2i{ 2, 1 }, MaterialType::WOOD, 0);

    // Print detailed header.
    std::cout << "\nFrame | Pos  | COM           | Velocity      | Pending Force | Sup\n";
    std::cout << "------|------|---------------|---------------|---------------|----\n";

    // Run for 50 frames, checking for movement.
    for (int frame = 1; frame <= 50; ++frame) {
        world->advanceTime(0.016);
        tracker.recordFrame(frame);

        if (tracker.checkForDisplacements(frame)) {
            FAIL() << "Wood moved unexpectedly at frame " << frame;
        }

        // Print detailed state every 5 frames (and first/last).
        if (frame == 1 || frame == 50 || frame % 5 == 0) {
            for (uint32_t y = 0; y < 3; ++y) {
                for (uint32_t x = 0; x < 3; ++x) {
                    const Cell& cell = world->getData().at(x, y);
                    if (cell.material_type == MaterialType::WOOD) {
                        std::cout << std::setw(5) << frame << " | (" << x << "," << y << ") | ("
                                  << std::setw(5) << std::fixed << std::setprecision(2)
                                  << cell.com.x << "," << std::setw(5) << cell.com.y << ") | ("
                                  << std::setw(5) << cell.velocity.x << "," << std::setw(5)
                                  << cell.velocity.y << ") | (" << std::setw(5)
                                  << cell.pending_force.x << "," << std::setw(5)
                                  << cell.pending_force.y << ") | "
                                  << (cell.has_any_support ? "Y" : "N")
                                  << (cell.has_vertical_support ? "v" : "h") << "\n";
                    }
                }
            }
        }
    }

    std::cout << "\n✅ SUCCESS! Cantilever stayed stable for 50 frames!\n";
    std::cout << WorldDiagramGeneratorEmoji::generateEmojiDiagram(*world) << "\n";
}
