#pragma once

#include "Cell.h"
#include "bitmaps/CellBitmap.h"

#include <vector>

namespace DirtSim {

/**
 * GridOfCells: Computed cache layer for World physics optimization.
 *
 * Design:
 * - Owns a COPY of the cell grid (rebuilt fresh each frame).
 * - Computes emptyCell bitmap for fast lookups.
 * - Valid from start of advanceTime() until processMaterialMoves().
 * - Compile-time toggle to switch between old/new lookup approach.
 *
 * Usage:
 *   GridOfCells grid(world.data.cells, width, height);
 *   if (grid.emptyCells().isSet(x, y)) { ... }  // Check if cell is empty.
 */
class GridOfCells {
public:
    // Runtime toggle: Controls whether calculators use bitmap or direct cell access.
    // Set to false to benchmark overhead, true for optimized path.
    static bool USE_CACHE;

private:
    std::vector<Cell> cells_; // Owned copy of cells (for future use).
    CellBitmap empty_cells_;  // Bitmap: 1 = empty (fill_ratio == 0).
    uint32_t width_;
    uint32_t height_;

    void buildEmptyCellMap();

public:
    // Constructor: Copy cells and compute bitmap.
    GridOfCells(const std::vector<Cell>& source_cells, uint32_t width, uint32_t height);

    // Accessor to empty cells bitmap.
    const CellBitmap& emptyCells() const { return empty_cells_; }

    // Grid dimensions.
    uint32_t getWidth() const { return width_; }
    uint32_t getHeight() const { return height_; }
};

} // namespace DirtSim
