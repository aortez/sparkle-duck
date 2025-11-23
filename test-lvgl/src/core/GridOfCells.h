#pragma once

#include "Cell.h"
#include "Vector2d.h"
#include "bitmaps/CellBitmap.h"
#include "bitmaps/EmptyNeighborhood.h"
#include "bitmaps/MaterialNeighborhood.h"

#include <vector>

// Forward declaration (Timers is in global namespace).
class Timers;

namespace DirtSim {

/**
 * GridOfCells: Computed cache layer for World physics optimization.
 *
 * Design:
 * - Owns a COPY of the cell grid (rebuilt fresh each frame).
 * - Computes emptyCell bitmap for fast lookups.
 * - Precomputes material neighborhoods for zero-lookup material queries.
 * - Valid from start of advanceTime() until processMaterialMoves().
 * - Compile-time toggle to switch between old/new lookup approach.
 *
 * Usage:
 *   GridOfCells grid(world.getData().cells, width, height, timers);
 *   if (grid.emptyCells().isSet(x, y)) { ... }  // Check if cell is empty.
 */
class GridOfCells {
public:
    // Runtime toggle: Controls whether calculators use bitmap or direct cell access.
    // Set to false to benchmark overhead, true for optimized path.
    static bool USE_CACHE;

private:
    std::vector<Cell> cells_;
    CellBitmap empty_cells_;
    CellBitmap support_bitmap_;
    std::vector<uint64_t> empty_neighborhoods_;
    std::vector<uint64_t> material_neighborhoods_;
    uint32_t width_;
    uint32_t height_;

    void buildEmptyCellMap();
    void precomputeEmptyNeighborhoods();
    void precomputeMaterialNeighborhoods();

public:
    // Constructor: Copy cells and compute bitmaps.
    GridOfCells(
        const std::vector<Cell>& source_cells, uint32_t width, uint32_t height, Timers& timers);

    const CellBitmap& emptyCells() const { return empty_cells_; }
    const CellBitmap& supportBitmap() const { return support_bitmap_; }
    CellBitmap& supportBitmap() { return support_bitmap_; }

    EmptyNeighborhood getEmptyNeighborhood(uint32_t x, uint32_t y) const
    {
        return EmptyNeighborhood{ Neighborhood3x3{ empty_neighborhoods_[y * width_ + x] } };
    }

    MaterialNeighborhood getMaterialNeighborhood(uint32_t x, uint32_t y) const
    {
        return MaterialNeighborhood{ material_neighborhoods_[y * width_ + x] };
    }

    // Grid dimensions.
    uint32_t getWidth() const { return width_; }
    uint32_t getHeight() const { return height_; }
    uint32_t getBlocksX() const { return empty_cells_.getBlocksX(); }
    uint32_t getBlocksY() const { return empty_cells_.getBlocksY(); }
};

} // namespace DirtSim
