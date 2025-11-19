#include "core/bitmaps/Neighborhood3x3.h"

#include <gtest/gtest.h>

using namespace DirtSim;

// Test bit position constants.
TEST(Neighborhood3x3Test, BitPositionConstants)
{
    EXPECT_EQ(Neighborhood3x3::NW, 0);
    EXPECT_EQ(Neighborhood3x3::N, 1);
    EXPECT_EQ(Neighborhood3x3::NE, 2);
    EXPECT_EQ(Neighborhood3x3::W, 3);
    EXPECT_EQ(Neighborhood3x3::C, 4);
    EXPECT_EQ(Neighborhood3x3::E, 5);
    EXPECT_EQ(Neighborhood3x3::SW, 6);
    EXPECT_EQ(Neighborhood3x3::S, 7);
    EXPECT_EQ(Neighborhood3x3::SE, 8);
}

// Test layer extraction.
TEST(Neighborhood3x3Test, LayerExtraction)
{
    // Create neighborhood with specific bit pattern.
    // Value layer: 0b101010101 (alternating)
    // Valid layer: 0b111111111 (all valid)
    uint64_t data = 0b101010101 | (0b111111111ULL << 9);
    Neighborhood3x3 n{ data };

    EXPECT_EQ(n.getValueLayer(), 0b101010101);
    EXPECT_EQ(n.getValidLayer(), 0b111111111);
}

// Test coordinate-based access.
TEST(Neighborhood3x3Test, CoordinateBasedAccess)
{
    // Set only center and cardinal directions.
    uint64_t data = 0;
    data |= (1 << Neighborhood3x3::N); // North.
    data |= (1 << Neighborhood3x3::S); // South.
    data |= (1 << Neighborhood3x3::E); // East.
    data |= (1 << Neighborhood3x3::W); // West.
    data |= (1 << Neighborhood3x3::C); // Center.

    // All valid.
    data |= (0b111111111ULL << 9);

    Neighborhood3x3 n{ data };

    // Test coordinate-based access.
    EXPECT_TRUE(n.getAt(0, -1)); // North.
    EXPECT_TRUE(n.getAt(0, 1));  // South.
    EXPECT_TRUE(n.getAt(1, 0));  // East.
    EXPECT_TRUE(n.getAt(-1, 0)); // West.
    EXPECT_TRUE(n.getAt(0, 0));  // Center.

    EXPECT_FALSE(n.getAt(-1, -1)); // NW.
    EXPECT_FALSE(n.getAt(1, -1));  // NE.
    EXPECT_FALSE(n.getAt(-1, 1));  // SW.
    EXPECT_FALSE(n.getAt(1, 1));   // SE.
}

// Test named accessors.
TEST(Neighborhood3x3Test, NamedAccessors)
{
    uint64_t data = 0;
    data |= (1 << Neighborhood3x3::N);
    data |= (1 << Neighborhood3x3::S);
    data |= (0b111111111ULL << 9); // All valid.

    Neighborhood3x3 n{ data };

    EXPECT_TRUE(n.north());
    EXPECT_TRUE(n.south());
    EXPECT_FALSE(n.east());
    EXPECT_FALSE(n.west());
    EXPECT_FALSE(n.center());
}

// Test validity accessors.
TEST(Neighborhood3x3Test, ValidityAccessors)
{
    // Only center and cardinal directions are valid.
    uint64_t data = 0;
    data |= ((1 << Neighborhood3x3::N) << 9);
    data |= ((1 << Neighborhood3x3::S) << 9);
    data |= ((1 << Neighborhood3x3::E) << 9);
    data |= ((1 << Neighborhood3x3::W) << 9);
    data |= ((1 << Neighborhood3x3::C) << 9);

    Neighborhood3x3 n{ data };

    EXPECT_TRUE(n.northValid());
    EXPECT_TRUE(n.southValid());
    EXPECT_TRUE(n.eastValid());
    EXPECT_TRUE(n.westValid());
    EXPECT_TRUE(n.centerValid());

    EXPECT_FALSE(n.northEastValid());
    EXPECT_FALSE(n.northWestValid());
    EXPECT_FALSE(n.southEastValid());
    EXPECT_FALSE(n.southWestValid());
}

// Test edge case: corner cell with OOB neighbors.
TEST(Neighborhood3x3Test, CornerCellWithOOBNeighbors)
{
    // Simulate top-left corner cell (0, 0).
    // Only SE, S, E, and C are valid.
    uint64_t data = 0;

    // Set values for valid cells.
    data |= (1 << Neighborhood3x3::C); // Center has material.
    data |= (1 << Neighborhood3x3::S); // South has material.

    // Set validity bits (only SE, S, E, C valid).
    data |= ((1 << Neighborhood3x3::SE) << 9);
    data |= ((1 << Neighborhood3x3::S) << 9);
    data |= ((1 << Neighborhood3x3::E) << 9);
    data |= ((1 << Neighborhood3x3::C) << 9);

    Neighborhood3x3 n{ data };

    // Check valid neighbors.
    EXPECT_TRUE(n.centerValid());
    EXPECT_TRUE(n.southValid());
    EXPECT_TRUE(n.eastValid());
    EXPECT_TRUE(n.southEastValid());

    // Check invalid (OOB) neighbors.
    EXPECT_FALSE(n.northValid());
    EXPECT_FALSE(n.westValid());
    EXPECT_FALSE(n.northWestValid());
    EXPECT_FALSE(n.northEastValid());
    EXPECT_FALSE(n.southWestValid());

    // Check values.
    EXPECT_TRUE(n.center());
    EXPECT_TRUE(n.south());
    EXPECT_FALSE(n.east());
}

// Test utility methods.
TEST(Neighborhood3x3Test, UtilityMethods)
{
    uint64_t data = 0;

    // Set 5 neighbors as valid and true.
    data |= (1 << Neighborhood3x3::N);
    data |= (1 << Neighborhood3x3::S);
    data |= (1 << Neighborhood3x3::E);
    data |= (1 << Neighborhood3x3::W);
    data |= (1 << Neighborhood3x3::NE);

    // All 9 cells valid.
    data |= (0b111111111ULL << 9);

    Neighborhood3x3 n{ data };

    EXPECT_EQ(n.countValidNeighbors(), 8);   // All 8 neighbors valid.
    EXPECT_EQ(n.countTrueNeighbors(), 5);    // 5 neighbors have property.
    EXPECT_FALSE(n.allValidNeighborsTrue()); // Not all are true.
}
