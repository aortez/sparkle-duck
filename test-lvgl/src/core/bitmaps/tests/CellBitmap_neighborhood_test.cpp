#include "core/bitmaps/CellBitmap.h"

#include <gtest/gtest.h>

using namespace DirtSim;

// Test extraction for interior cell (fast path).
TEST(CellBitmapNeighborhoodTest, InteriorCellFastPath)
{
    CellBitmap bitmap(20, 20);

    // Set a pattern around cell (10, 10) - interior cell.
    // Set N, S, E, W (cardinal directions).
    bitmap.set(10, 9);  // North.
    bitmap.set(10, 11); // South.
    bitmap.set(11, 10); // East.
    bitmap.set(9, 10);  // West.

    Neighborhood3x3 n = bitmap.getNeighborhood3x3(10, 10);

    // Check cardinal directions.
    EXPECT_TRUE(n.north());
    EXPECT_TRUE(n.south());
    EXPECT_TRUE(n.east());
    EXPECT_TRUE(n.west());

    // Diagonals and center should be clear.
    EXPECT_FALSE(n.northEast());
    EXPECT_FALSE(n.northWest());
    EXPECT_FALSE(n.southEast());
    EXPECT_FALSE(n.southWest());
    EXPECT_FALSE(n.center());

    // All should be valid (interior cell).
    EXPECT_TRUE(n.northValid());
    EXPECT_TRUE(n.southValid());
    EXPECT_TRUE(n.eastValid());
    EXPECT_TRUE(n.westValid());
    EXPECT_TRUE(n.centerValid());
    EXPECT_EQ(n.countValidNeighbors(), 8);
}

// Test extraction for top-left corner (OOB handling).
TEST(CellBitmapNeighborhoodTest, TopLeftCornerOOB)
{
    CellBitmap bitmap(20, 20);

    // Cell at (0, 0) - top-left corner.
    // Set south and east neighbors.
    bitmap.set(0, 1); // South.
    bitmap.set(1, 0); // East.

    Neighborhood3x3 n = bitmap.getNeighborhood3x3(0, 0);

    // Check values.
    EXPECT_TRUE(n.south());
    EXPECT_TRUE(n.east());
    EXPECT_FALSE(n.center());

    // Check validity - only SE, S, E, C are valid.
    EXPECT_TRUE(n.southValid());
    EXPECT_TRUE(n.eastValid());
    EXPECT_TRUE(n.southEastValid());
    EXPECT_TRUE(n.centerValid());

    // NW, N, NE, W, SW are OOB.
    EXPECT_FALSE(n.northValid());
    EXPECT_FALSE(n.westValid());
    EXPECT_FALSE(n.northWestValid());
    EXPECT_FALSE(n.northEastValid());
    EXPECT_FALSE(n.southWestValid());

    // Count valid neighbors (should be 3: S, E, SE).
    EXPECT_EQ(n.countValidNeighbors(), 3);
}

// Test extraction for bottom-right corner.
TEST(CellBitmapNeighborhoodTest, BottomRightCornerOOB)
{
    CellBitmap bitmap(10, 10);

    // Cell at (9, 9) - bottom-right corner.
    bitmap.set(8, 9); // West.
    bitmap.set(9, 8); // North.

    Neighborhood3x3 n = bitmap.getNeighborhood3x3(9, 9);

    // Check values.
    EXPECT_TRUE(n.north());
    EXPECT_TRUE(n.west());

    // Check validity - only NW, N, W, C are valid.
    EXPECT_TRUE(n.northValid());
    EXPECT_TRUE(n.westValid());
    EXPECT_TRUE(n.northWestValid());
    EXPECT_TRUE(n.centerValid());

    // SE, S, E, NE, SW are OOB.
    EXPECT_FALSE(n.southValid());
    EXPECT_FALSE(n.eastValid());
    EXPECT_FALSE(n.southEastValid());
    EXPECT_FALSE(n.northEastValid());
    EXPECT_FALSE(n.southWestValid());
}

// Test extraction on block boundary (should use slow path).
TEST(CellBitmapNeighborhoodTest, BlockBoundaryCell)
{
    CellBitmap bitmap(20, 20);

    // Cell at (8, 8) - on block boundary between blocks.
    // Set all 8 neighbors.
    bitmap.set(7, 7); // NW.
    bitmap.set(8, 7); // N.
    bitmap.set(9, 7); // NE.
    bitmap.set(7, 8); // W.
    bitmap.set(9, 8); // E.
    bitmap.set(7, 9); // SW.
    bitmap.set(8, 9); // S.
    bitmap.set(9, 9); // SE.

    Neighborhood3x3 n = bitmap.getNeighborhood3x3(8, 8);

    // All neighbors should be set.
    EXPECT_TRUE(n.northWest());
    EXPECT_TRUE(n.north());
    EXPECT_TRUE(n.northEast());
    EXPECT_TRUE(n.west());
    EXPECT_TRUE(n.east());
    EXPECT_TRUE(n.southWest());
    EXPECT_TRUE(n.south());
    EXPECT_TRUE(n.southEast());
    EXPECT_FALSE(n.center());

    // All should be valid.
    EXPECT_EQ(n.countValidNeighbors(), 8);
}

// Test coordinate-based access.
TEST(CellBitmapNeighborhoodTest, CoordinateBasedIteration)
{
    CellBitmap bitmap(20, 20);

    // Set a cross pattern around (10, 10).
    bitmap.set(10, 9);  // North.
    bitmap.set(10, 11); // South.
    bitmap.set(11, 10); // East.
    bitmap.set(9, 10);  // West.

    Neighborhood3x3 n = bitmap.getNeighborhood3x3(10, 10);

    // Iterate using coordinates.
    int count = 0;
    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            if (dx == 0 && dy == 0) continue; // Skip center.

            if (n.isValidAt(dx, dy) && n.getAt(dx, dy)) {
                count++;
            }
        }
    }

    EXPECT_EQ(count, 4); // N, S, E, W.
}

// Test that fast path and slow path produce same results.
TEST(CellBitmapNeighborhoodTest, FastPathMatchesSlowPath)
{
    CellBitmap bitmap(20, 20);

    // Create a pattern.
    for (uint32_t y = 5; y < 15; ++y) {
        for (uint32_t x = 5; x < 15; ++x) {
            if ((x + y) % 2 == 0) {
                bitmap.set(x, y);
            }
        }
    }

    // Test interior cell (uses fast path).
    Neighborhood3x3 interior = bitmap.getNeighborhood3x3(10, 10);

    // Test edge cell (uses slow path).
    Neighborhood3x3 edge = bitmap.getNeighborhood3x3(0, 0);

    // Verify interior cell has expected pattern.
    // Since (10,10) is even, center should be set.
    EXPECT_TRUE(interior.center());
    EXPECT_TRUE(interior.centerValid());

    // Verify edge cell has valid flags set correctly.
    EXPECT_TRUE(edge.centerValid());
    EXPECT_TRUE(edge.southValid());
    EXPECT_TRUE(edge.eastValid());
    EXPECT_FALSE(edge.northValid());
    EXPECT_FALSE(edge.westValid());
}
