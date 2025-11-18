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

} // namespace DirtSim
