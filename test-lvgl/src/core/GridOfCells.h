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

private:
    std::vector<Cell>& cells_;
    CellBitmap empty_cells_;
    CellBitmap support_bitmap_;
    std::vector<uint64_t> empty_neighborhoods_;
    std::vector<uint64_t> material_neighborhoods_;
    std::vector<double> cohesion_resistance_; // Frame-scoped cohesion resistance cache.
    uint32_t width_;
    uint32_t height_;

    void buildEmptyCellMap();
    void precomputeEmptyNeighborhoods();
    void precomputeMaterialNeighborhoods();

public:
    // Constructor: Reference cells and compute bitmaps (no copy).
    GridOfCells(std::vector<Cell>& cells, uint32_t width, uint32_t height, Timers& timers);

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

    // Cohesion resistance cache (computed during applyCohesionForces, used in resolveForces).
    void setCohesionResistance(uint32_t x, uint32_t y, double resistance)
    {
        cohesion_resistance_[y * width_ + x] = resistance;
    }

    double getCohesionResistance(uint32_t x, uint32_t y) const
    {
        return cohesion_resistance_[y * width_ + x];
    }

    // Direct cell access (use grid instead of world for cell lookups).
    Cell& at(uint32_t x, uint32_t y) { return cells_[y * width_ + x]; }

    const Cell& at(uint32_t x, uint32_t y) const { return cells_[y * width_ + x]; }

    std::vector<Cell>& getCells() { return cells_; }

    const std::vector<Cell>& getCells() const { return cells_; }

    // Grid dimensions.
    uint32_t getWidth() const { return width_; }
    uint32_t getHeight() const { return height_; }
    uint32_t getBlocksX() const { return empty_cells_.getBlocksX(); }
    uint32_t getBlocksY() const { return empty_cells_.getBlocksY(); }
};

} // namespace DirtSim
