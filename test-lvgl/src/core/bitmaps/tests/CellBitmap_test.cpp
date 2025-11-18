#include "core/bitmaps/CellBitmap.h"

#include <gtest/gtest.h>

using namespace DirtSim;

// Test basic construction.
TEST(CellBitmapTest, ConstructionInitializesAllBitsToZero)
{
    CellBitmap bitmap(100, 100);

    // All bits should be zero initially.
    for (uint32_t y = 0; y < 100; ++y) {
        for (uint32_t x = 0; x < 100; ++x) {
            EXPECT_FALSE(bitmap.isSet(x, y)) << "Bit at (" << x << ", " << y << ") should be zero";
        }
    }
}

// Test set/clear operations.
TEST(CellBitmapTest, SetAndClearOperations)
{
    CellBitmap bitmap(100, 100);

    // Set a bit.
    bitmap.set(50, 50);
    EXPECT_TRUE(bitmap.isSet(50, 50));

    // Clear the bit.
    bitmap.clear(50, 50);
    EXPECT_FALSE(bitmap.isSet(50, 50));
}

// Test multiple bits in same block.
TEST(CellBitmapTest, MultipleBitsInSameBlock)
{
    CellBitmap bitmap(100, 100);

    // Set multiple bits in the same 8×8 block (0-7, 0-7).
    bitmap.set(0, 0);
    bitmap.set(7, 0);
    bitmap.set(0, 7);
    bitmap.set(7, 7);

    EXPECT_TRUE(bitmap.isSet(0, 0));
    EXPECT_TRUE(bitmap.isSet(7, 0));
    EXPECT_TRUE(bitmap.isSet(0, 7));
    EXPECT_TRUE(bitmap.isSet(7, 7));

    // Other bits in same block should still be zero.
    EXPECT_FALSE(bitmap.isSet(1, 1));
    EXPECT_FALSE(bitmap.isSet(5, 5));
}

// Test bits in different blocks.
TEST(CellBitmapTest, BitsInDifferentBlocks)
{
    CellBitmap bitmap(100, 100);

    // Set bits in different 8×8 blocks.
    bitmap.set(0, 0);   // Block (0, 0).
    bitmap.set(8, 0);   // Block (1, 0).
    bitmap.set(0, 8);   // Block (0, 1).
    bitmap.set(8, 8);   // Block (1, 1).
    bitmap.set(50, 50); // Block (6, 6).

    EXPECT_TRUE(bitmap.isSet(0, 0));
    EXPECT_TRUE(bitmap.isSet(8, 0));
    EXPECT_TRUE(bitmap.isSet(0, 8));
    EXPECT_TRUE(bitmap.isSet(8, 8));
    EXPECT_TRUE(bitmap.isSet(50, 50));

    // Adjacent bits should be zero.
    EXPECT_FALSE(bitmap.isSet(1, 0));
    EXPECT_FALSE(bitmap.isSet(7, 0));
    EXPECT_FALSE(bitmap.isSet(9, 0));
}

// Test edge cases: boundary cells.
TEST(CellBitmapTest, BoundaryCells)
{
    CellBitmap bitmap(100, 100);

    // Test corners.
    bitmap.set(0, 0);
    bitmap.set(99, 0);
    bitmap.set(0, 99);
    bitmap.set(99, 99);

    EXPECT_TRUE(bitmap.isSet(0, 0));
    EXPECT_TRUE(bitmap.isSet(99, 0));
    EXPECT_TRUE(bitmap.isSet(0, 99));
    EXPECT_TRUE(bitmap.isSet(99, 99));
}

// Test partial blocks (grid not multiple of 8).
TEST(CellBitmapTest, PartialBlocks)
{
    CellBitmap bitmap(10, 10); // 10×10 requires 2×2 blocks, last block is partial.

    // Set bits in partial block.
    bitmap.set(9, 9);
    EXPECT_TRUE(bitmap.isSet(9, 9));

    bitmap.set(8, 9);
    EXPECT_TRUE(bitmap.isSet(8, 9));
}

// Test set/clear doesn't affect other bits.
TEST(CellBitmapTest, SetClearDoesNotAffectOtherBits)
{
    CellBitmap bitmap(100, 100);

    // Set a pattern.
    bitmap.set(10, 10);
    bitmap.set(20, 20);
    bitmap.set(30, 30);

    // Clear one bit.
    bitmap.clear(20, 20);

    // Others should remain set.
    EXPECT_TRUE(bitmap.isSet(10, 10));
    EXPECT_FALSE(bitmap.isSet(20, 20));
    EXPECT_TRUE(bitmap.isSet(30, 30));
}

// Test dimensions.
TEST(CellBitmapTest, Dimensions)
{
    CellBitmap bitmap(123, 456);

    EXPECT_EQ(bitmap.getWidth(), 123);
    EXPECT_EQ(bitmap.getHeight(), 456);
}
