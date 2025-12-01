#include "WorldSupportCalculator.h"
#include "Cell.h"
#include "GridOfCells.h"
#include "MaterialType.h"
#include "PhysicsSettings.h"
#include "Vector2i.h"
#include "World.h"
#include "WorldData.h"
#include "spdlog/spdlog.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <queue>
#include <set>

using namespace DirtSim;

WorldSupportCalculator::WorldSupportCalculator(GridOfCells& grid) : grid_(grid)
{}

bool WorldSupportCalculator::hasVerticalSupport(const World& world, uint32_t x, uint32_t y) const
{
    if (x >= grid_.getWidth() || y >= grid_.getHeight()) {
        spdlog::trace("hasVerticalSupport({},{}) = false (invalid cell)", x, y);
        return false;
    }

    const Cell& cell = grid_.at(x, y);
    if (cell.isEmpty()) {
        spdlog::trace("hasVerticalSupport({},{}) = false (empty cell)", x, y);
        return false;
    }

    // Check if already at ground level.
    if (y == grid_.getHeight() - 1) {
        spdlog::trace("hasVerticalSupport({},{}) = true (at ground level)", x, y);
        return true;
    }

    // Check cells directly below for continuous material (no gaps allowed).
    for (uint32_t dy = 1; dy <= MAX_VERTICAL_SUPPORT_DISTANCE; dy++) {
        uint32_t support_y = y + dy;

        // If we reach beyond the world boundary, no material support available.
        if (support_y >= grid_.getHeight()) {
            spdlog::info(
                "hasVerticalSupport({},{}) = false (reached world boundary at distance {}, no "
                "material below)",
                x,
                y,
                dy);
            break;
        }

        const Cell& below = grid_.at(x, support_y);
        if (!below.isEmpty()) {
            // RECURSIVE SUPPORT CHECK: Supporting block must itself be supported.
            bool supporting_block_supported = hasVerticalSupport(world, x, support_y);
            if (supporting_block_supported) {
                spdlog::trace(
                    "hasVerticalSupport({},{}) = true (material {} below at distance {} is itself "
                    "supported)",
                    x,
                    y,
                    getMaterialName(below.material_type),
                    dy);
                return true;
            }
            else {
                spdlog::trace(
                    "hasVerticalSupport({},{}) = false (material {} below at distance {} is "
                    "unsupported)",
                    x,
                    y,
                    getMaterialName(below.material_type),
                    dy);
                return false;
            }
        }
        else {
            // Stop at first empty cell - no support through gaps.
            spdlog::trace(
                "hasVerticalSupport({},{}) = false (empty cell at distance {}, no continuous "
                "support)",
                x,
                y,
                dy);
            return false;
        }
    }

    spdlog::info(
        "hasVerticalSupport({},{}) = false (no material below within {} cells)",
        x,
        y,
        MAX_VERTICAL_SUPPORT_DISTANCE);
    return false;
}

bool WorldSupportCalculator::hasHorizontalSupport(
    const uint32_t x,
    const uint32_t y,
    const EmptyNeighborhood& empty_n,
    const MaterialNeighborhood& mat_n) const
{
    if (!empty_n.centerHasMaterial()) {
        return false;
    }

    const uint16_t neighbor_mask = empty_n.getMaterialNeighborsBitGrid() & ~(1 << 4);
    if (neighbor_mask == 0) {
        return false;
    }

    const MaterialType center_mat = mat_n.getCenterMaterial();
    const MaterialProperties& cell_props = getMaterialProperties(center_mat);

    // Cell must be rigid to provide/receive horizontal support.
    if (!cell_props.is_rigid) {
        spdlog::trace(
            "hasHorizontalSupport({},{}) = false (center {} is not rigid)",
            x,
            y,
            getMaterialName(center_mat));
        return false;
    }

    for (int bit_pos = 0; bit_pos < 9; ++bit_pos) {
        if (!(neighbor_mask & (1 << bit_pos))) continue;

        const MaterialType neighbor_mat = mat_n.getMaterialByBitPos(bit_pos);
        const MaterialProperties& neighbor_props = getMaterialProperties(neighbor_mat);

        // Neighbor must be rigid to provide support.
        if (!neighbor_props.is_rigid) {
            continue;
        }

        // Same material: use cohesion for structural bonding.
        if (neighbor_mat == center_mat) {
            if (cell_props.cohesion > COHESION_SUPPORT_THRESHOLD) {
                spdlog::debug(
                    "hasHorizontalSupport({},{}) = true (rigid same-material {} neighbor with "
                    "cohesion {:.3f})",
                    x,
                    y,
                    getMaterialName(neighbor_mat),
                    cell_props.cohesion);
                return true;
            }
        }
        // Different materials: use adhesion for cross-material bonding.
        else {
            const double mutual_adhesion = std::sqrt(cell_props.adhesion * neighbor_props.adhesion);

            if (mutual_adhesion > ADHESION_SUPPORT_THRESHOLD) {
                spdlog::debug(
                    "hasHorizontalSupport({},{}) = true (rigid different-material {} neighbor with "
                    "adhesion {:.3f})",
                    x,
                    y,
                    getMaterialName(neighbor_mat),
                    mutual_adhesion);
                return true;
            }
        }
    }

    spdlog::trace(
        "hasHorizontalSupport({},{}) = false (no rigid neighbors with strong cohesion/adhesion)",
        x,
        y);
    return false;
}

bool WorldSupportCalculator::hasHorizontalSupport(const World& world, uint32_t x, uint32_t y) const
{
    if (GridOfCells::USE_CACHE) {
        const EmptyNeighborhood empty_n = grid_.getEmptyNeighborhood(x, y);
        const MaterialNeighborhood mat_n = grid_.getMaterialNeighborhood(x, y);
        return hasHorizontalSupport(x, y, empty_n, mat_n);
    }
    else {
        // ========== DIRECT PATH: Traditional cell access ==========
        if (!isValidCell(world, static_cast<int>(x), static_cast<int>(y))) {
            spdlog::trace("hasHorizontalSupport({},{}) = false (invalid cell)", x, y);
            return false;
        }

        const Cell& cell = getCellAt(world, x, y);
        if (cell.isEmpty()) {
            spdlog::trace("hasHorizontalSupport({},{}) = false (empty cell)", x, y);
            return false;
        }

        const MaterialProperties& cell_props = getMaterialProperties(cell.material_type);

        // Cell must be rigid to provide/receive horizontal support.
        if (!cell_props.is_rigid) {
            spdlog::trace(
                "hasHorizontalSupport({},{}) = false (center {} is not rigid)",
                x,
                y,
                getMaterialName(cell.material_type));
            return false;
        }

        // Check immediate neighbors only (no BFS for horizontal support).
        for (int dx = -1; dx <= 1; dx++) {
            for (int dy = -1; dy <= 1; dy++) {
                if (dx == 0 && dy == 0) continue; // Skip self.

                int nx = static_cast<int>(x) + dx;
                int ny = static_cast<int>(y) + dy;

                if (!isValidCell(world, nx, ny)) continue;

                const Cell& neighbor = getCellAt(world, nx, ny);
                if (neighbor.isEmpty()) continue;

                const MaterialProperties& neighbor_props =
                    getMaterialProperties(neighbor.material_type);

                // Neighbor must be rigid to provide support.
                if (!neighbor_props.is_rigid) {
                    continue;
                }

                // Same material: use cohesion for structural bonding.
                if (neighbor.material_type == cell.material_type) {
                    if (cell_props.cohesion > COHESION_SUPPORT_THRESHOLD) {
                        spdlog::debug(
                            "hasHorizontalSupport({},{}) = true (rigid same-material {} neighbor "
                            "with cohesion {:.3f})",
                            x,
                            y,
                            getMaterialName(neighbor.material_type),
                            cell_props.cohesion);
                        return true;
                    }
                }
                // Different materials: use adhesion for cross-material bonding.
                else {
                    double mutual_adhesion =
                        std::sqrt(cell_props.adhesion * neighbor_props.adhesion);

                    if (mutual_adhesion > ADHESION_SUPPORT_THRESHOLD) {
                        spdlog::debug(
                            "hasHorizontalSupport({},{}) = true (rigid different-material {} "
                            "neighbor with adhesion {:.3f})",
                            x,
                            y,
                            getMaterialName(neighbor.material_type),
                            mutual_adhesion);
                        return true;
                    }
                }
            }
        }

        spdlog::trace(
            "hasHorizontalSupport({},{}) = false (no rigid neighbors with strong "
            "cohesion/adhesion)",
            x,
            y);
        return false;
    }
}

bool WorldSupportCalculator::hasStructuralSupport(uint32_t x, uint32_t y) const
{
    const Cell& cell = grid_.at(x, y);

    // Empty cells provide no support.
    if (cell.isEmpty()) {
        return false;
    }

    // Support conditions (in order of priority):

    // 1. WALL material is always considered structurally supported.
    if (cell.material_type == MaterialType::WALL) {
        return true;
    }

    // 2. Bottom edge of world (ground) provides support.
    if (y == grid_.getHeight() - 1) {
        return true;
    }

    // 3. High-density materials provide structural support.
    // METAL has density 7.8, so it acts as structural anchor.
    const MaterialProperties& props = getMaterialProperties(cell.material_type);
    if (props.density > 5.0) {
        return true;
    }

    // 4. NEW: Limited-depth BFS to find nearby structural support.
    // Check within MAX_SUPPORT_DISTANCE (10 cells) for ground/walls/anchors.
    std::queue<std::pair<Vector2i, int>> search_queue; // position, distance.
    std::set<std::pair<int, int>> visited;             // Track visited cells to avoid cycles.

    search_queue.push({ { static_cast<int>(x), static_cast<int>(y) }, 0 });
    visited.insert({ static_cast<int>(x), static_cast<int>(y) });

    while (!search_queue.empty()) {
        auto [pos, distance] = search_queue.front();
        search_queue.pop();

        // Skip if we've exceeded maximum search distance.
        if (distance >= static_cast<int>(MAX_SUPPORT_DISTANCE)) {
            continue;
        }

        // Check all 8 neighbors from current position.
        for (int dx = -1; dx <= 1; dx++) {
            for (int dy = -1; dy <= 1; dy++) {
                if (dx == 0 && dy == 0) continue; // Skip self.

                int nx = pos.x + dx;
                int ny = pos.y + dy;

                if (nx < 0 || ny < 0 || nx >= static_cast<int>(grid_.getWidth())
                    || ny >= static_cast<int>(grid_.getHeight()) || visited.count({ nx, ny })) {
                    continue;
                }

                visited.insert({ nx, ny });
                const Cell& neighbor = grid_.at(nx, ny);

                // Skip empty cells.
                if (neighbor.isEmpty()) {
                    continue;
                }

                // Check for immediate structural support.
                // Walls only provide support to rigid materials, not fluids.
                if (neighbor.material_type == MaterialType::WALL) {
                    // Only rigid materials can be structurally supported by walls.
                    const MaterialProperties& cell_props =
                        getMaterialProperties(cell.material_type);
                    if (cell_props.is_rigid) {
                        return true;
                    }
                    // Fluids adjacent to walls are NOT structurally supported.
                }
                // Ground level provides support to all materials.
                else if (ny == static_cast<int>(grid_.getHeight()) - 1) {
                    return true;
                }

                // High-density materials act as anchors.
                const MaterialProperties& neighbor_props =
                    getMaterialProperties(neighbor.material_type);
                if (neighbor_props.density > 5.0) {
                    spdlog::trace(
                        "hasStructuralSupport({},{}) = true (found high-density {} at distance {})",
                        x,
                        y,
                        getMaterialName(neighbor.material_type),
                        distance + 1);
                    return true;
                }

                // Continue BFS only through connected materials (same type)
                // This prevents "floating through air" false positives.
                if (neighbor.material_type == cell.material_type
                    && neighbor.fill_ratio > MIN_MATTER_THRESHOLD) {
                    search_queue.push({ { nx, ny }, distance + 1 });
                }
            }
        }
    }

    spdlog::trace(
        "hasStructuralSupport({},{}) = false (no support found within {} cells)",
        x,
        y,
        MAX_SUPPORT_DISTANCE);
    return false;
}

void WorldSupportCalculator::computeSupportMapBottomUp(World& world) const
{
    (void)world;
    if (GridOfCells::USE_CACHE) {
        CellBitmap& support = grid_.supportBitmap();

        for (int y = grid_.getHeight() - 1; y >= 0; y--) {
            for (uint32_t x = 0; x < grid_.getWidth(); x++) {
                const EmptyNeighborhood empty_n = grid_.getEmptyNeighborhood(x, y);

                if (!empty_n.centerHasMaterial()) {
                    support.clear(x, y);
                    grid_.at(x, y).has_vertical_support = false;
                    grid_.at(x, y).has_any_support = false;
                    continue;
                }

                const MaterialNeighborhood mat_n = grid_.getMaterialNeighborhood(x, y);

                if (mat_n.getCenterMaterial() == MaterialType::WALL) {
                    support.set(x, y);
                    grid_.at(x, y).has_vertical_support = true;
                    grid_.at(x, y).has_any_support = true;
                    continue;
                }

                if (y == static_cast<int>(grid_.getHeight()) - 1) {
                    support.set(x, y);
                    grid_.at(x, y).has_vertical_support = true;
                    grid_.at(x, y).has_any_support = true;
                    continue;
                }

                // Fluids (WATER, AIR) don't provide vertical support.
                const MaterialType below_material = mat_n.getMaterial(0, 1); // South = (0, +1)
                const bool below_is_fluid =
                    (below_material == MaterialType::WATER || below_material == MaterialType::AIR);
                const bool has_vertical =
                    empty_n.southHasMaterial() && support.isSet(x, y + 1) && !below_is_fluid;

                // Set support fields - horizontal support will be added by propagation passes.
                grid_.at(x, y).has_vertical_support = has_vertical;
                grid_.at(x, y).has_any_support = has_vertical;
                if (has_vertical) {
                    support.set(x, y);
                }
                else {
                    support.clear(x, y);
                }
            }
        }

        // ========== HORIZONTAL PROPAGATION PASSES ==========
        // Propagate support through rigid connections (cantilever beams, etc.).
        // Two passes ensure support flows in all horizontal directions: O(2n) = O(n).

        // Pass 2: Left-to-right propagation.
        for (uint32_t y = 0; y < grid_.getHeight(); y++) {
            for (uint32_t x = 1; x < grid_.getWidth(); x++) {
                Cell& cell = grid_.at(x, y);
                if (cell.has_any_support || cell.isEmpty()) {
                    continue; // Already supported or empty.
                }

                const MaterialProperties& cell_props = getMaterialProperties(cell.material_type);
                if (!cell_props.is_rigid) {
                    continue; // Only rigid materials propagate support.
                }

                // Check left neighbor.
                const Cell& left = grid_.at(x - 1, y);
                if (!left.has_any_support || left.isEmpty()) {
                    continue;
                }

                const MaterialProperties& left_props = getMaterialProperties(left.material_type);
                if (!left_props.is_rigid) {
                    continue; // Neighbor must also be rigid.
                }

                // Check bond strength (cohesion for same material, adhesion for different).
                bool strong_bond = false;
                if (left.material_type == cell.material_type) {
                    strong_bond = (cell_props.cohesion > COHESION_SUPPORT_THRESHOLD);
                }
                else {
                    const double mutual_adhesion =
                        std::sqrt(cell_props.adhesion * left_props.adhesion);
                    strong_bond = (mutual_adhesion > ADHESION_SUPPORT_THRESHOLD);
                }

                if (strong_bond) {
                    // Propagate support from left neighbor.
                    cell.has_any_support = true;
                    grid_.supportBitmap().set(x, y);
                    spdlog::debug(
                        "Support propagated L->R: ({},{}) {} got support from ({},{}) {}",
                        x,
                        y,
                        getMaterialName(cell.material_type),
                        x - 1,
                        y,
                        getMaterialName(left.material_type));
                }
            }
        }

        // Pass 3: Right-to-left propagation.
        for (uint32_t y = 0; y < grid_.getHeight(); y++) {
            for (int x = grid_.getWidth() - 2; x >= 0; x--) {
                Cell& cell = grid_.at(x, y);
                if (cell.has_any_support || cell.isEmpty()) {
                    continue; // Already supported or empty.
                }

                const MaterialProperties& cell_props = getMaterialProperties(cell.material_type);
                if (!cell_props.is_rigid) {
                    continue; // Only rigid materials propagate support.
                }

                // Check right neighbor.
                const Cell& right = grid_.at(x + 1, y);
                if (!right.has_any_support || right.isEmpty()) {
                    continue;
                }

                const MaterialProperties& right_props = getMaterialProperties(right.material_type);
                if (!right_props.is_rigid) {
                    continue; // Neighbor must also be rigid.
                }

                // Check bond strength (cohesion for same material, adhesion for different).
                bool strong_bond = false;
                if (right.material_type == cell.material_type) {
                    strong_bond = (cell_props.cohesion > COHESION_SUPPORT_THRESHOLD);
                }
                else {
                    const double mutual_adhesion =
                        std::sqrt(cell_props.adhesion * right_props.adhesion);
                    strong_bond = (mutual_adhesion > ADHESION_SUPPORT_THRESHOLD);
                }

                if (strong_bond) {
                    // Propagate support from right neighbor.
                    cell.has_any_support = true;
                    grid_.supportBitmap().set(x, y);
                }
            }
        }

        // Pass 4: Top-to-bottom propagation.
        for (uint32_t y = 1; y < grid_.getHeight(); y++) {
            for (uint32_t x = 0; x < grid_.getWidth(); x++) {
                Cell& cell = grid_.at(x, y);
                if (cell.has_any_support || cell.isEmpty()) {
                    continue; // Already supported or empty.
                }

                const MaterialProperties& cell_props = getMaterialProperties(cell.material_type);
                if (!cell_props.is_rigid) {
                    continue; // Only rigid materials propagate support.
                }

                // Check up neighbor.
                const Cell& up = grid_.at(x, y - 1);
                if (!up.has_any_support || up.isEmpty()) {
                    continue;
                }

                const MaterialProperties& up_props = getMaterialProperties(up.material_type);
                if (!up_props.is_rigid) {
                    continue; // Neighbor must also be rigid.
                }

                // Check bond strength.
                bool strong_bond = false;
                if (up.material_type == cell.material_type) {
                    strong_bond = (cell_props.cohesion > COHESION_SUPPORT_THRESHOLD);
                }
                else {
                    const double mutual_adhesion =
                        std::sqrt(cell_props.adhesion * up_props.adhesion);
                    strong_bond = (mutual_adhesion > ADHESION_SUPPORT_THRESHOLD);
                }

                if (strong_bond) {
                    // Propagate support from up neighbor.
                    cell.has_any_support = true;
                    grid_.supportBitmap().set(x, y);
                }
            }
        }

        // Pass 5: Bottom-to-top propagation.
        for (int y = grid_.getHeight() - 2; y >= 0; y--) {
            for (uint32_t x = 0; x < grid_.getWidth(); x++) {
                Cell& cell = grid_.at(x, y);
                if (cell.has_any_support || cell.isEmpty()) {
                    continue; // Already supported or empty.
                }

                const MaterialProperties& cell_props = getMaterialProperties(cell.material_type);
                if (!cell_props.is_rigid) {
                    continue; // Only rigid materials propagate support.
                }

                // Check down neighbor.
                const Cell& down = grid_.at(x, y + 1);
                if (!down.has_any_support || down.isEmpty()) {
                    continue;
                }

                const MaterialProperties& down_props = getMaterialProperties(down.material_type);
                if (!down_props.is_rigid) {
                    continue; // Neighbor must also be rigid.
                }

                // Check bond strength.
                bool strong_bond = false;
                if (down.material_type == cell.material_type) {
                    strong_bond = (cell_props.cohesion > COHESION_SUPPORT_THRESHOLD);
                }
                else {
                    const double mutual_adhesion =
                        std::sqrt(cell_props.adhesion * down_props.adhesion);
                    strong_bond = (mutual_adhesion > ADHESION_SUPPORT_THRESHOLD);
                }

                if (strong_bond) {
                    // Propagate support from down neighbor.
                    cell.has_any_support = true;
                    grid_.supportBitmap().set(x, y);
                }
            }
        }
    }
    else {
        // ========== DIRECT PATH: Traditional cell access ==========
        // Bottom-up pass: compute support for entire grid in one sweep.
        // Start from bottom row (ground) and work upward.
        for (int y = grid_.getHeight() - 1; y >= 0; y--) {
            for (uint32_t x = 0; x < grid_.getWidth(); x++) {
                Cell& cell = grid_.at(x, y);

                // Skip AIR cells - they don't participate in structural support.
                if (cell.material_type == MaterialType::AIR) {
                    cell.has_any_support = false;
                    cell.has_vertical_support = false;
                    continue;
                }

                // WALL material is always structurally supported.
                if (cell.material_type == MaterialType::WALL) {
                    cell.has_any_support = true;
                    cell.has_vertical_support = true;
                    continue;
                }

                // Bottom edge of world (ground) provides support.
                if (y == static_cast<int>(grid_.getHeight()) - 1) {
                    cell.has_any_support = true;
                    cell.has_vertical_support = true;
                    continue;
                }

                // Check vertical support: cell below must be non-empty, supported, AND not a fluid.
                // Fluids (WATER, AIR) don't provide vertical support.
                const Cell& below = grid_.at(x, y + 1);
                const bool below_is_fluid =
                    (below.material_type == MaterialType::WATER
                     || below.material_type == MaterialType::AIR);
                bool has_vertical = !below.isEmpty() && below.has_any_support && !below_is_fluid;

                // Set support fields - horizontal support will be added by propagation passes.
                cell.has_vertical_support = has_vertical;
                cell.has_any_support = has_vertical;
            }
        }

        // ========== HORIZONTAL PROPAGATION PASSES (DIRECT PATH) ==========
        // Propagate support through rigid connections (cantilever beams, etc.).
        // Two passes ensure support flows in all horizontal directions: O(2n) = O(n).

        // Pass 2: Left-to-right propagation.
        for (uint32_t y = 0; y < grid_.getHeight(); y++) {
            for (uint32_t x = 1; x < grid_.getWidth(); x++) {
                Cell& cell = grid_.at(x, y);
                if (cell.has_any_support || cell.isEmpty()) {
                    continue; // Already supported or empty.
                }

                const MaterialProperties& cell_props = getMaterialProperties(cell.material_type);
                if (!cell_props.is_rigid) {
                    continue; // Only rigid materials propagate support.
                }

                // Check left neighbor.
                const Cell& left = grid_.at(x - 1, y);
                if (!left.has_any_support || left.isEmpty()) {
                    continue;
                }

                const MaterialProperties& left_props = getMaterialProperties(left.material_type);
                if (!left_props.is_rigid) {
                    continue; // Neighbor must also be rigid.
                }

                // Check bond strength (cohesion for same material, adhesion for different).
                bool strong_bond = false;
                if (left.material_type == cell.material_type) {
                    strong_bond = (cell_props.cohesion > COHESION_SUPPORT_THRESHOLD);
                }
                else {
                    const double mutual_adhesion =
                        std::sqrt(cell_props.adhesion * left_props.adhesion);
                    strong_bond = (mutual_adhesion > ADHESION_SUPPORT_THRESHOLD);
                }

                if (strong_bond) {
                    // Propagate support from left neighbor.
                    cell.has_any_support = true;
                }
            }
        }

        // Pass 3: Right-to-left propagation.
        for (uint32_t y = 0; y < grid_.getHeight(); y++) {
            for (int x = grid_.getWidth() - 2; x >= 0; x--) {
                Cell& cell = grid_.at(x, y);
                if (cell.has_any_support || cell.isEmpty()) {
                    continue; // Already supported or empty.
                }

                const MaterialProperties& cell_props = getMaterialProperties(cell.material_type);
                if (!cell_props.is_rigid) {
                    continue; // Only rigid materials propagate support.
                }

                // Check right neighbor.
                const Cell& right = grid_.at(x + 1, y);
                if (!right.has_any_support || right.isEmpty()) {
                    continue;
                }

                const MaterialProperties& right_props = getMaterialProperties(right.material_type);
                if (!right_props.is_rigid) {
                    continue; // Neighbor must also be rigid.
                }

                // Check bond strength (cohesion for same material, adhesion for different).
                bool strong_bond = false;
                if (right.material_type == cell.material_type) {
                    strong_bond = (cell_props.cohesion > COHESION_SUPPORT_THRESHOLD);
                }
                else {
                    const double mutual_adhesion =
                        std::sqrt(cell_props.adhesion * right_props.adhesion);
                    strong_bond = (mutual_adhesion > ADHESION_SUPPORT_THRESHOLD);
                }

                if (strong_bond) {
                    // Propagate support from right neighbor.
                    cell.has_any_support = true;
                }
            }
        }

        // Pass 4: Top-to-bottom propagation.
        for (uint32_t y = 1; y < grid_.getHeight(); y++) {
            for (uint32_t x = 0; x < grid_.getWidth(); x++) {
                Cell& cell = grid_.at(x, y);
                if (cell.has_any_support || cell.isEmpty()) {
                    continue; // Already supported or empty.
                }

                const MaterialProperties& cell_props = getMaterialProperties(cell.material_type);
                if (!cell_props.is_rigid) {
                    continue; // Only rigid materials propagate support.
                }

                // Check up neighbor.
                const Cell& up = grid_.at(x, y - 1);
                if (!up.has_any_support || up.isEmpty()) {
                    continue;
                }

                const MaterialProperties& up_props = getMaterialProperties(up.material_type);
                if (!up_props.is_rigid) {
                    continue; // Neighbor must also be rigid.
                }

                // Check bond strength.
                bool strong_bond = false;
                if (up.material_type == cell.material_type) {
                    strong_bond = (cell_props.cohesion > COHESION_SUPPORT_THRESHOLD);
                }
                else {
                    const double mutual_adhesion =
                        std::sqrt(cell_props.adhesion * up_props.adhesion);
                    strong_bond = (mutual_adhesion > ADHESION_SUPPORT_THRESHOLD);
                }

                if (strong_bond) {
                    // Propagate support from up neighbor.
                    cell.has_any_support = true;
                }
            }
        }

        // Pass 5: Bottom-to-top propagation.
        for (int y = grid_.getHeight() - 2; y >= 0; y--) {
            for (uint32_t x = 0; x < grid_.getWidth(); x++) {
                Cell& cell = grid_.at(x, y);
                if (cell.has_any_support || cell.isEmpty()) {
                    continue; // Already supported or empty.
                }

                const MaterialProperties& cell_props = getMaterialProperties(cell.material_type);
                if (!cell_props.is_rigid) {
                    continue; // Only rigid materials propagate support.
                }

                // Check down neighbor.
                const Cell& down = grid_.at(x, y + 1);
                if (!down.has_any_support || down.isEmpty()) {
                    continue;
                }

                const MaterialProperties& down_props = getMaterialProperties(down.material_type);
                if (!down_props.is_rigid) {
                    continue; // Neighbor must also be rigid.
                }

                // Check bond strength.
                bool strong_bond = false;
                if (down.material_type == cell.material_type) {
                    strong_bond = (cell_props.cohesion > COHESION_SUPPORT_THRESHOLD);
                }
                else {
                    const double mutual_adhesion =
                        std::sqrt(cell_props.adhesion * down_props.adhesion);
                    strong_bond = (mutual_adhesion > ADHESION_SUPPORT_THRESHOLD);
                }

                if (strong_bond) {
                    // Propagate support from down neighbor.
                    cell.has_any_support = true;
                }
            }
        }
    }
}

double WorldSupportCalculator::calculateDistanceToSupport(
    const World& world, uint32_t x, uint32_t y) const
{
    spdlog::info("calculateDistanceToSupport({},{}) called", x, y);
    const Cell& cell = getCellAt(world, x, y);
    if (cell.isEmpty()) {
        spdlog::info(
            "calculateDistanceToSupport({},{}) = {} (empty cell)", x, y, MAX_SUPPORT_DISTANCE);
        return MAX_SUPPORT_DISTANCE; // No material = no support needed.
    }

    MaterialType material = cell.material_type;

    // Use simpler 2D array for distance tracking (avoid Vector2i comparisons)
    std::vector<std::vector<int>> distances(
        world.getData().width, std::vector<int>(world.getData().height, -1));
    std::queue<std::pair<uint32_t, uint32_t>> queue;

    queue.push({ x, y });
    distances[x][y] = 0;

    // 8-directional neighbor offsets (including diagonals)
    static const std::array<std::pair<int, int>, 8> directions = {
        { { -1, -1 }, { -1, 0 }, { -1, 1 }, { 0, -1 }, { 0, 1 }, { 1, -1 }, { 1, 0 }, { 1, 1 } }
    };

    while (!queue.empty()) {
        auto current = queue.front();
        queue.pop();

        uint32_t cx = current.first;
        uint32_t cy = current.second;

        // Check if current position has structural support.
        if (hasStructuralSupport(cx, cy)) {
            int distance = distances[cx][cy];
            spdlog::trace("Support found for material at ({},{}) - distance: {}", x, y, distance);
            return static_cast<double>(distance);
        }

        // Limit search depth to prevent infinite loops and improve performance.
        if (distances[cx][cy] >= static_cast<int>(MAX_SUPPORT_DISTANCE)) {
            continue;
        }

        // Explore all 8 neighbors.
        for (const auto& dir : directions) {
            int nx = static_cast<int>(cx) + dir.first;
            int ny = static_cast<int>(cy) + dir.second;

            if (nx >= 0 && ny >= 0 && nx < static_cast<int>(world.getData().width)
                && ny < static_cast<int>(world.getData().height)
                && distances[nx][ny] == -1) { // Not visited.

                const Cell& nextCell = getCellAt(world, nx, ny);

                // Follow paths through connected material.
                // Either same material, or structural support material (metal, walls)
                bool canConnect = false;
                if (nextCell.material_type == material
                    && nextCell.fill_ratio > MIN_MATTER_THRESHOLD) {
                    canConnect = true; // Same material connection.
                }
                else if (!nextCell.isEmpty() && hasStructuralSupport(nx, ny)) {
                    canConnect = true; // Structural support connection (metal, walls, etc.)
                }

                if (canConnect) {
                    distances[nx][ny] = distances[cx][cy] + 1;
                    queue.push({ static_cast<uint32_t>(nx), static_cast<uint32_t>(ny) });
                }
            }
        }
    }

    // No support found within search radius.
    spdlog::trace(
        "No support found for material at ({},{}) within distance {}", x, y, MAX_SUPPORT_DISTANCE);
    return MAX_SUPPORT_DISTANCE;
}
