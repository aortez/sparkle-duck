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
    uint32_t block_x = x / BLOCK_SIZE;
    uint32_t block_y = y / BLOCK_SIZE;
    block_idx = block_y * blocks_x_ + block_x;

    // Local coordinates within the 8×8 block.
    int local_x = x % BLOCK_SIZE;
    int local_y = y % BLOCK_SIZE;

    // Bit index: row-major order (y * 8 + x).
    bit_idx = local_y * BLOCK_SIZE + local_x;
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

} // namespace DirtSim
