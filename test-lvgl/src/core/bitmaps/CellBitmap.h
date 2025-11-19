#pragma once

#include "Neighborhood3x3.h"

#include <cstdint>
#include <vector>

namespace DirtSim {

/**
 * Generic bit-packed grid for tracking boolean cell properties.
 * Uses 8×8 block representation inspired by chess bitboards.
 *
 * Can track any boolean property: empty cells, active cells, etc.
 *
 * Bit mapping within each uint64_t block (row-major):
 *   Bit 0-7:   Row 0 (y=0), x increasing left to right
 *   Bit 8-15:  Row 1 (y=1)
 *   ...
 *   Bit 56-63: Row 7 (y=7)
 */
class CellBitmap {
private:
    uint32_t grid_width_;
    uint32_t grid_height_;
    uint32_t blocks_x_; // Number of 8×8 blocks horizontally.
    uint32_t blocks_y_; // Number of 8×8 blocks vertically.
    std::vector<uint64_t> blocks_;

    static constexpr int BLOCK_SIZE = 8;

    // Convert cell coordinates to block index and bit index.
    inline void cellToBlockAndBit(uint32_t x, uint32_t y, uint32_t& block_idx, int& bit_idx) const;

public:
    CellBitmap(uint32_t width, uint32_t height);

    // Core bit operations.
    void set(uint32_t x, uint32_t y);
    void clear(uint32_t x, uint32_t y);
    bool isSet(uint32_t x, uint32_t y) const;

    // Block-level operations.
    uint64_t getBlock(uint32_t block_x, uint32_t block_y) const;
    bool isBlockAllSet(uint32_t block_x, uint32_t block_y) const;   // All bits = 1.
    bool isBlockAllClear(uint32_t block_x, uint32_t block_y) const; // All bits = 0.

    // Neighborhood extraction.
    Neighborhood3x3 getNeighborhood3x3(uint32_t x, uint32_t y) const;

    // Grid dimensions.
    uint32_t getWidth() const { return grid_width_; }
    uint32_t getHeight() const { return grid_height_; }
    uint32_t getBlocksX() const { return blocks_x_; }
    uint32_t getBlocksY() const { return blocks_y_; }
};

} // namespace DirtSim
