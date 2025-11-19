#include "GridOfCells.h"

#include "spdlog/spdlog.h"

namespace DirtSim {

// Runtime toggle for cache usage (default: enabled).
bool GridOfCells::USE_CACHE = true;

GridOfCells::GridOfCells(const std::vector<Cell>& source_cells, uint32_t width, uint32_t height)
    : cells_(source_cells), // Copy cells.
      empty_cells_(width, height),
      empty_neighborhoods_(width * height, 0),
      width_(width),
      height_(height)
{
    spdlog::debug("GridOfCells: Constructing cache ({}x{})", width, height);
    buildEmptyCellMap();
    precomputeEmptyNeighborhoods();
    spdlog::debug("GridOfCells: Construction complete");
}

void GridOfCells::buildEmptyCellMap()
{
    // Scan all cells and mark empty ones in bitmap.
    for (uint32_t y = 0; y < height_; ++y) {
        for (uint32_t x = 0; x < width_; ++x) {
            const Cell& cell = cells_[y * width_ + x];

            if (cell.isEmpty()) {
                empty_cells_.set(x, y);
            }
        }
    }
}

void GridOfCells::precomputeEmptyNeighborhoods()
{
    // Precompute 3×3 neighborhood for every cell.
    for (uint32_t y = 0; y < height_; ++y) {
        for (uint32_t x = 0; x < width_; ++x) {
            Neighborhood3x3 n = empty_cells_.getNeighborhood3x3(x, y);
            empty_neighborhoods_[y * width_ + x] = n.data;
        }
    }
}

} // namespace DirtSim
