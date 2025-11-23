#include "GridOfCells.h"

#include "ScopeTimer.h"
#include "spdlog/spdlog.h"

namespace DirtSim {

// Runtime toggle for cache usage (default: enabled).
bool GridOfCells::USE_CACHE = true;

GridOfCells::GridOfCells(
    const std::vector<Cell>& source_cells, uint32_t width, uint32_t height, Timers& timers)
    : cells_(source_cells),
      empty_cells_(width, height),
      support_bitmap_(width, height),
      empty_neighborhoods_(width * height, 0),
      material_neighborhoods_(width * height, 0),
      cohesion_resistance_(width * height, 0.0),
      width_(width),
      height_(height)
{
    spdlog::debug("GridOfCells: Constructing cache ({}x{})", width, height);

    {
        ScopeTimer timer(timers, "grid_cache_empty_map");
        buildEmptyCellMap();
    }

    {
        ScopeTimer timer(timers, "grid_cache_empty_neighborhoods");
        precomputeEmptyNeighborhoods();
    }

    {
        ScopeTimer timer(timers, "grid_cache_material_neighborhoods");
        precomputeMaterialNeighborhoods();
    }

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

void GridOfCells::precomputeMaterialNeighborhoods()
{
    // Precompute 3×3 material neighborhood for every cell.
    for (uint32_t y = 0; y < height_; ++y) {
        for (uint32_t x = 0; x < width_; ++x) {
            uint64_t packed = 0;

            // Pack 9 material types (4 bits each) into uint64_t.
            for (int dy = -1; dy <= 1; ++dy) {
                for (int dx = -1; dx <= 1; ++dx) {
                    int bit_group = (dy + 1) * 3 + (dx + 1); // 0-8
                    int nx = static_cast<int>(x) + dx;
                    int ny = static_cast<int>(y) + dy;

                    MaterialType mat = MaterialType::AIR; // Default for OOB.
                    if (nx >= 0 && nx < static_cast<int>(width_) && ny >= 0
                        && ny < static_cast<int>(height_)) {
                        const Cell& cell = cells_[ny * width_ + nx];
                        mat = cell.material_type;
                    }

                    // Pack material type (4 bits) into position.
                    uint64_t mat_bits = static_cast<uint64_t>(mat) & 0xF;
                    packed |= (mat_bits << (bit_group * 4));
                }
            }

            material_neighborhoods_[y * width_ + x] = packed;
        }
    }
}

} // namespace DirtSim
