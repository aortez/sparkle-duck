#include "CellBitmap.h"

namespace DirtSim {

CellBitmap::CellBitmap(uint32_t width, uint32_t height) : grid_width_(width), grid_height_(height)
{
    // Calculate number of 8×8 blocks needed (round up for partial blocks).
    blocks_x_ = (width + BLOCK_SIZE - 1) / BLOCK_SIZE;
    blocks_y_ = (height + BLOCK_SIZE - 1) / BLOCK_SIZE;

    // Allocate one uint64_t per 8×8 block, initialized to zero.
    blocks_.resize(blocks_x_ * blocks_y_, 0);
}

inline void CellBitmap::cellToBlockAndBit(
    uint32_t x, uint32_t y, uint32_t& block_idx, int& bit_idx) const
{
    // Block coordinates using bit shifts instead of division.
    uint32_t block_x = x >> 3; // x / 8
    uint32_t block_y = y >> 3; // y / 8
    block_idx = block_y * blocks_x_ + block_x;

    // Local coordinates within the 8×8 block using bit masks instead of modulo.
    int local_x = x & 7; // x % 8 (7 = 0b111)
    int local_y = y & 7; // y % 8

    // Bit index using shift instead of multiplication: row-major order (y * 8 + x).
    bit_idx = (local_y << 3) | local_x; // local_y * 8 + local_x
}

void CellBitmap::set(uint32_t x, uint32_t y)
{
    uint32_t block_idx;
    int bit_idx;
    cellToBlockAndBit(x, y, block_idx, bit_idx);
    blocks_[block_idx] |= (1ULL << bit_idx);
}

void CellBitmap::clear(uint32_t x, uint32_t y)
{
    uint32_t block_idx;
    int bit_idx;
    cellToBlockAndBit(x, y, block_idx, bit_idx);
    blocks_[block_idx] &= ~(1ULL << bit_idx);
}

bool CellBitmap::isSet(uint32_t x, uint32_t y) const
{
    uint32_t block_idx;
    int bit_idx;
    cellToBlockAndBit(x, y, block_idx, bit_idx);
    return (blocks_[block_idx] >> bit_idx) & 1;
}

uint64_t CellBitmap::getBlock(uint32_t block_x, uint32_t block_y) const
{
    uint32_t block_idx = block_y * blocks_x_ + block_x;
    return blocks_[block_idx];
}

bool CellBitmap::isBlockAllSet(uint32_t block_x, uint32_t block_y) const
{
    return getBlock(block_x, block_y) == 0xFFFFFFFFFFFFFFFFULL;
}

bool CellBitmap::isBlockAllClear(uint32_t block_x, uint32_t block_y) const
{
    return getBlock(block_x, block_y) == 0;
}

Neighborhood3x3 CellBitmap::getNeighborhood3x3(uint32_t x, uint32_t y) const
{
    uint32_t block_x = x >> 3;
    uint32_t block_y = y >> 3;
    int local_x = x & 7;
    int local_y = y & 7;

    // Fast path: 3×3 fits entirely within one 8×8 block.
    // Requires cell to be at least 1 cell away from block edges.
    if (local_x >= 1 && local_x <= 6 && local_y >= 1 && local_y <= 6) {
        // Also check grid boundaries.
        if (x >= 1 && x < grid_width_ - 1 && y >= 1 && y < grid_height_ - 1) {
            uint64_t block = getBlock(block_x, block_y);

            // Extract 3 rows of 3 bits.
            int base_bit = ((local_y - 1) << 3) | (local_x - 1);

            uint16_t row0 = (block >> base_bit) & 0b111;
            uint16_t row1 = (block >> (base_bit + 8)) & 0b111;
            uint16_t row2 = (block >> (base_bit + 16)) & 0b111;

            uint64_t value_layer = row0 | (row1 << 3) | (row2 << 6);

            // All 9 cells are in-bounds (interior cell).
            uint64_t valid_layer = 0x1FFULL;

            return Neighborhood3x3{ value_layer | (valid_layer << 9) };
        }
    }

    // Slow path: Near block boundaries or grid edges.
    uint64_t result = 0;

    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            int bit_pos = (dy + 1) * 3 + (dx + 1);
            int nx = static_cast<int>(x) + dx;
            int ny = static_cast<int>(y) + dy;

            // Check grid boundaries.
            if (nx >= 0 && nx < static_cast<int>(grid_width_) && ny >= 0
                && ny < static_cast<int>(grid_height_)) {
                // Set validity bit.
                result |= (1ULL << (9 + bit_pos));

                // Set value bit if property is true.
                if (isSet(nx, ny)) {
                    result |= (1ULL << bit_pos);
                }
            }
            // OOB cells: validity=0, value=0 (default).
        }
    }

    return Neighborhood3x3{ result };
}

} // namespace DirtSim
