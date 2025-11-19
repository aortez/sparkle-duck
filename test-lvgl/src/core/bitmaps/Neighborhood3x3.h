#pragma once

#include <array>
#include <cstdint>
#include <utility>

namespace DirtSim {

/**
 * Utility functions for working with 3×3 neighborhood bit grids.
 *
 * These helpers are useful when you need to convert between bit positions
 * and spatial offsets (dx, dy) from the center of a 3×3 grid.
 */
namespace Neighborhood3x3Utils {

/**
 * Lookup table: 3×3 bit position → (dx, dy) offset from center.
 *
 * Bit layout (row-major):
 *   0  1  2    →    NW  N  NE
 *   3  4  5    →    W   C  E
 *   6  7  8    →    SW  S  SE
 *
 * Offsets are relative to center:
 *   dx: -1 (left), 0 (center), +1 (right)
 *   dy: -1 (top),  0 (center), +1 (bottom)
 *
 * Usage:
 *   int bit_pos = 1;  // North
 *   auto [dx, dy] = BIT_TO_OFFSET[bit_pos];  // (0, -1)
 *
 * Note: For most 3×3 iterations, a simple nested loop is faster:
 *   for (int dy = -1; dy <= 1; dy++)
 *     for (int dx = -1; dx <= 1; dx++)
 *       int bit_pos = (dy + 1) * 3 + (dx + 1);
 *
 * Use this table when you have a bit position and need the offset,
 * not when iterating through all positions.
 */
static constexpr std::array<std::pair<int8_t, int8_t>, 9> BIT_TO_OFFSET = { {
    { -1, -1 },
    { 0, -1 },
    { 1, -1 }, // Bits 0-2: Row 0 (NW, N, NE)
    { -1, 0 },
    { 0, 0 },
    { 1, 0 }, // Bits 3-5: Row 1 (W,  C, E)
    { -1, 1 },
    { 0, 1 },
    { 1, 1 } // Bits 6-8: Row 2 (SW, S, SE)
} };

} // namespace Neighborhood3x3Utils

/**
 * 3×3 neighborhood extracted from CellBitmap.
 *
 * Packs 9 property values + 9 validity flags into uint64_t:
 *   Bits 0-8:   Property values (1 = true, e.g., isEmpty)
 *   Bits 9-17:  Validity flags (1 = in-bounds, 0 = OOB)
 *   Bits 18-63: Unused (46 bits for future use)
 *
 * Bit layout for 3×3 grid:
 *   NW N  NE     Bit positions:
 *   W  C  E      0  1  2
 *   SW S  SE     3  4  5
 *                6  7  8
 */
struct Neighborhood3x3 {
    uint64_t data;

    // ========== Bit Position Constants ==========
    static constexpr int NW = 0, N = 1, NE = 2;
    static constexpr int W = 3, C = 4, E = 5;
    static constexpr int SW = 6, S = 7, SE = 8;

    // ========== Layer Extraction ==========
    uint16_t getValueLayer() const { return data & 0x1FF; }
    uint16_t getValidLayer() const { return (data >> 9) & 0x1FF; }

    // ========== Coordinate-Based Access (for iteration) ==========
    // dx, dy in range [-1, 1].
    bool getAt(int dx, int dy) const
    {
        int bit_pos = (dy + 1) * 3 + (dx + 1);
        return (data >> bit_pos) & 1;
    }

    bool isValidAt(int dx, int dy) const
    {
        int bit_pos = (dy + 1) * 3 + (dx + 1);
        return (data >> (9 + bit_pos)) & 1;
    }

    // Check if neighbor exists and has property.
    bool hasAt(int dx, int dy) const { return isValidAt(dx, dy) && getAt(dx, dy); }

    // ========== Named Accessors (for readability) ==========
    // Value accessors.
    bool north() const { return (data >> N) & 1; }
    bool south() const { return (data >> S) & 1; }
    bool east() const { return (data >> E) & 1; }
    bool west() const { return (data >> W) & 1; }
    bool center() const { return (data >> C) & 1; }
    bool northEast() const { return (data >> NE) & 1; }
    bool northWest() const { return (data >> NW) & 1; }
    bool southEast() const { return (data >> SE) & 1; }
    bool southWest() const { return (data >> SW) & 1; }

    // Validity accessors.
    bool northValid() const { return (data >> (9 + N)) & 1; }
    bool southValid() const { return (data >> (9 + S)) & 1; }
    bool eastValid() const { return (data >> (9 + E)) & 1; }
    bool westValid() const { return (data >> (9 + W)) & 1; }
    bool centerValid() const { return (data >> (9 + C)) & 1; }
    bool northEastValid() const { return (data >> (9 + NE)) & 1; }
    bool northWestValid() const { return (data >> (9 + NW)) & 1; }
    bool southEastValid() const { return (data >> (9 + SE)) & 1; }
    bool southWestValid() const { return (data >> (9 + SW)) & 1; }

    // ========== Utility Methods ==========
    // Count valid neighbors (excluding center).
    int countValidNeighbors() const
    {
        uint16_t valid = getValidLayer();
        return __builtin_popcount(valid & ~(1 << C));
    }

    // Count neighbors with property set (excluding center).
    int countTrueNeighbors() const
    {
        uint16_t values = getValueLayer();
        return __builtin_popcount(values & ~(1 << C));
    }

    // Check if all valid neighbors have property.
    bool allValidNeighborsTrue() const
    {
        uint16_t valid = getValidLayer() & ~(1 << C);
        uint16_t values = getValueLayer() & ~(1 << C);
        return (values & valid) == valid;
    }
};

} // namespace DirtSim
