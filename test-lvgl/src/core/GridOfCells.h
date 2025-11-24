#pragma once

#include "Cell.h"
#include "CellDebug.h"
#include "Vector2d.h"
#include "bitmaps/CellBitmap.h"
#include "bitmaps/EmptyNeighborhood.h"
#include "bitmaps/MaterialNeighborhood.h"

#include <vector>

namespace DirtSim {

/**
 * GridOfCells: Computed cache layer for World physics optimization.
 *
 * Design:
 * - Holds reference to World's cell grid.
 * - Computes emptyCell bitmap for fast lookups.
 * - Precomputes material neighborhoods for zero-lookup material queries.
 * - Provides direct cell access to eliminate World indirection.
 * - Valid from start of advanceTime() until processMaterialMoves().
 * - Compile-time toggle to switch between old/new lookup approach.
 *
 * Usage:
 *   GridOfCells grid(world.getData().cells, width, height, timers);
 *   Cell& cell = grid.at(x, y);  // Direct access, no World needed.
 *   if (grid.emptyCells().isSet(x, y)) { ... }  // Check if cell is empty.
 */
class GridOfCells {
public:
    // Runtime toggle: Controls whether calculators use bitmap or direct cell access.
    // Set to false to benchmark overhead, true for optimized path.
    static bool USE_CACHE;

    // Runtime toggle: Controls whether OpenMP parallelization is enabled.
    // Set to false to test sequential execution, true for parallel.
    static bool USE_OPENMP;

private:
    std::vector<Cell>& cells_;           // Reference to WorldData's cells.
    std::vector<CellDebug>& debug_info_; // Reference to WorldData's debug info.
    CellBitmap empty_cells_;
    CellBitmap wall_cells_;
    CellBitmap support_bitmap_;
    std::vector<uint64_t> empty_neighborhoods_;
    std::vector<uint64_t> material_neighborhoods_;
    uint32_t width_;
    uint32_t height_;

    void populateMaps();
    void buildEmptyCellMap();
    void buildWallCellMap();
    void precomputeEmptyNeighborhoods();
    void precomputeMaterialNeighborhoods();

public:
    GridOfCells(
        std::vector<Cell>& cells,
        std::vector<CellDebug>& debug_info,
        uint32_t width,
        uint32_t height);

    inline const CellBitmap& emptyCells() const { return empty_cells_; }
    inline const CellBitmap& wallCells() const { return wall_cells_; }
    inline const CellBitmap& supportBitmap() const { return support_bitmap_; }
    inline CellBitmap& supportBitmap() { return support_bitmap_; }

    inline EmptyNeighborhood getEmptyNeighborhood(uint32_t x, uint32_t y) const
    {
        return EmptyNeighborhood{ Neighborhood3x3{ empty_neighborhoods_[y * width_ + x] } };
    }

    inline MaterialNeighborhood getMaterialNeighborhood(uint32_t x, uint32_t y) const
    {
        return MaterialNeighborhood{ material_neighborhoods_[y * width_ + x] };
    }

    // Debug info access.
    inline CellDebug& debugAt(uint32_t x, uint32_t y) { return debug_info_[y * width_ + x]; }

    inline const CellDebug& debugAt(uint32_t x, uint32_t y) const
    {
        return debug_info_[y * width_ + x];
    }

    // Legacy cohesion resistance access (for compatibility).
    void setCohesionResistance(uint32_t x, uint32_t y, double resistance)
    {
        debug_info_[y * width_ + x].cohesion_resistance = resistance;
    }

    double getCohesionResistance(uint32_t x, uint32_t y) const
    {
        return debug_info_[y * width_ + x].cohesion_resistance;
    }

    inline Cell& at(uint32_t x, uint32_t y) { return cells_[y * width_ + x]; }

    inline const Cell& at(uint32_t x, uint32_t y) const { return cells_[y * width_ + x]; }

    inline std::vector<Cell>& getCells() { return cells_; }

    inline const std::vector<Cell>& getCells() const { return cells_; }

    // Grid dimensions.
    inline uint32_t getWidth() const { return width_; }
    inline uint32_t getHeight() const { return height_; }
    inline uint32_t getBlocksX() const { return empty_cells_.getBlocksX(); }
    inline uint32_t getBlocksY() const { return empty_cells_.getBlocksY(); }
};

} // namespace DirtSim
