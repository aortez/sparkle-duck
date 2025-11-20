#include "WorldCohesionCalculator.h"
#include "Cell.h"
#include "MaterialType.h"
#include "World.h"
#include "WorldSupportCalculator.h"
#include "spdlog/spdlog.h"
#include <algorithm>
#include <cmath>

using namespace DirtSim;

WorldCohesionCalculator::CohesionForce WorldCohesionCalculator::calculateCohesionForce(
    const World& world, uint32_t x, uint32_t y) const
{
    const Cell& cell = getCellAt(world, x, y);
    if (cell.isEmpty()) {
        return { 0.0, 0 };
    }

    const MaterialProperties& props = getMaterialProperties(cell.material_type);
    double material_cohesion = props.cohesion;
    uint32_t connected_neighbors = 0;

    // Check all 8 neighbors (including diagonals)
    for (int dx = -1; dx <= 1; dx++) {
        for (int dy = -1; dy <= 1; dy++) {
            if (dx == 0 && dy == 0) continue; // Skip self.

            int nx = static_cast<int>(x) + dx;
            int ny = static_cast<int>(y) + dy;

            if (isValidCell(world, nx, ny)) {
                const Cell& neighbor = getCellAt(world, nx, ny);
                if (neighbor.material_type == cell.material_type
                    && neighbor.fill_ratio > MIN_MATTER_THRESHOLD) {

                    // Weight by neighbor's fill ratio for partial cells.
                    connected_neighbors += 1; // Count as 1 neighbor.
                }
            }
        }
    }

    // Check for metal neighbors that provide structural support.
    uint32_t metal_neighbors = 0;
    if (cell.material_type == MaterialType::METAL) {
        // Count metal neighbors for structural support.
        for (int dx = -1; dx <= 1; dx++) {
            for (int dy = -1; dy <= 1; dy++) {
                if (dx == 0 && dy == 0) continue; // Skip self.

                int nx = static_cast<int>(x) + dx;
                int ny = static_cast<int>(y) + dy;

                if (isValidCell(world, nx, ny)) {
                    const Cell& neighbor = getCellAt(world, nx, ny);
                    if (neighbor.material_type == MaterialType::METAL
                        && neighbor.fill_ratio > 0.5) {
                        metal_neighbors++;
                    }
                }
            }
        }
    }

    bool is_supported = cell.has_support;

    double support_factor;

    // Metal with sufficient metal neighbors provides full structural support.
    if (metal_neighbors >= 2) {
        // Metal network provides full support in all directions.
        support_factor = 1.0;
        spdlog::trace(
            "Metal structural network support for {} at ({},{}) with {} metal neighbors",
            getMaterialName(cell.material_type),
            x,
            y,
            metal_neighbors);
    }
    else if (is_supported) {
        // Determine support type by checking cell below.
        bool has_vertical = false;
        if (y < world.data.height - 1) {
            const Cell& below = getCellAt(world, x, y + 1);
            has_vertical = !below.isEmpty() && below.has_support;
        }

        if (has_vertical) {
            // Full cohesion with vertical support (load-bearing from below).
            support_factor = 1.0;
            spdlog::trace(
                "Full vertical support for {} at ({},{})", getMaterialName(cell.material_type), x, y);
        }
        else {
            // Reduced cohesion with horizontal/other support (rigid lateral connections).
            support_factor = 0.5;
            spdlog::trace(
                "Horizontal support only for {} at ({},{})", getMaterialName(cell.material_type), x, y);
        }
    }
    else {
        // Minimal cohesion without structural support.
        support_factor = World::MIN_SUPPORT_FACTOR; // 0.05.
        spdlog::trace(
            "No structural support for {} at ({},{})", getMaterialName(cell.material_type), x, y);
    }

    // Resistance magnitude = cohesion × connection strength × own fill ratio × support factor.
    double resistance = material_cohesion * connected_neighbors * cell.fill_ratio * support_factor;

    spdlog::trace(
        "Cohesion calculation for {} at ({},{}): neighbors={}, has_support={}, "
        "support_factor={:.2f}, resistance={:.3f}",
        getMaterialName(cell.material_type),
        x,
        y,
        connected_neighbors,
        is_supported,
        support_factor,
        resistance);

    return { resistance, connected_neighbors };
}

WorldCohesionCalculator::COMCohesionForce WorldCohesionCalculator::calculateCOMCohesionForce(
    const World& world, uint32_t x, uint32_t y, uint32_t com_cohesion_range) const
{
    const Cell& cell = getCellAt(world, x, y);
    if (cell.isEmpty()) {
        return { { 0.0, 0.0 }, 0.0, { 0.0, 0.0 }, 0, 0.0, 0.0, false };
    }

    // Tunable force balance (adjust these to tune clustering vs stability).
    // Centering is primary (keeps COMs stable), clustering is secondary (gentle aggregation).
    static constexpr double CLUSTERING_WEIGHT = 0.5; // Weak neighbor attraction (dampening).
    static constexpr double CENTERING_WEIGHT = 1.0;  // Strong COM centering (stability).

    const double cell_mass = cell.getMass();
    const Vector2d com = cell.com;
    const Vector2d cell_world_pos(
        static_cast<double>(x) + cell.com.x, static_cast<double>(y) + cell.com.y);
    const MaterialProperties& props = getMaterialProperties(cell.material_type);

    // ===================================================================
    // FORCE 1: Clustering (attraction toward same-material neighbors)
    // ===================================================================

    Vector2d neighbor_center_sum(0.0, 0.0);
    double total_weight = 0.0;
    uint32_t connection_count = 0;

    // Check all neighbors within range for same-material connections.
    int range = static_cast<int>(com_cohesion_range);
    for (int dx = -range; dx <= range; dx++) {
        for (int dy = -range; dy <= range; dy++) {
            if (dx == 0 && dy == 0) continue;

            int nx = static_cast<int>(x) + dx;
            int ny = static_cast<int>(y) + dy;

            if (isValidCell(world, nx, ny)) {
                const Cell& neighbor = getCellAt(world, nx, ny);
                if (neighbor.material_type == cell.material_type
                    && neighbor.fill_ratio > MIN_MATTER_THRESHOLD) {

                    // Get neighbor's COM position in world coordinates.
                    Vector2d neighbor_world_pos(
                        static_cast<double>(nx) + neighbor.com.x,
                        static_cast<double>(ny) + neighbor.com.y);
                    double weight = neighbor.fill_ratio;

                    neighbor_center_sum += neighbor_world_pos * weight;
                    total_weight += weight;
                    connection_count++;
                }
            }
        }
    }

    Vector2d clustering_force(0.0, 0.0);
    Vector2d neighbor_center(0.0, 0.0);

    if (connection_count > 0 && total_weight > MIN_MATTER_THRESHOLD) {
        neighbor_center = neighbor_center_sum / total_weight;
        Vector2d to_neighbors = neighbor_center - cell_world_pos;
        double distance_sq = to_neighbors.x * to_neighbors.x + to_neighbors.y * to_neighbors.y;

        if (distance_sq > 0.000001) {
            double distance = std::sqrt(distance_sq);
            Vector2d clustering_direction = to_neighbors * (1.0 / distance);

            double distance_factor = 1.0 / (distance + 0.1);
            double max_connections = (2 * range + 1) * (2 * range + 1) - 1;

            // Mass-based factor: Uses total neighbor fill ratios (not just count).
            // This makes larger/fuller clusters pull harder than sparse ones.
            double mass_factor = total_weight / max_connections;

            double clustering_magnitude =
                props.cohesion * mass_factor * distance_factor * cell.fill_ratio;

            // Cap to prevent excessive forces.
            clustering_magnitude = std::min(clustering_magnitude, props.cohesion * 10.0);

            clustering_force = clustering_direction * clustering_magnitude * CLUSTERING_WEIGHT;
        }
    }

    // ===================================================================
    // FORCE 2: Centering (pull COM toward cell center for stability)
    // ===================================================================

    Vector2d centering_force(0.0, 0.0);
    Vector2d centering_direction(0.0, 0.0);
    double com_offset_sq = com.x * com.x + com.y * com.y;
    double com_offset = 0.0;

    if (com_offset_sq > 0.000001) {
        com_offset = std::sqrt(com_offset_sq);
        centering_direction = com * (-1.0 / com_offset);

        double centering_magnitude = props.cohesion * com_offset * cell.fill_ratio;

        centering_force = centering_direction * centering_magnitude * CENTERING_WEIGHT;
    }

    Vector2d final_force = centering_force;

    double clustering_force_sq = clustering_force.x * clustering_force.x + clustering_force.y * clustering_force.y;
    if (clustering_force_sq > 0.000001 && com_offset_sq > 0.000001) {
        Vector2d cell_grid_pos(static_cast<double>(x), static_cast<double>(y));
        Vector2d to_neighbors_vec = neighbor_center - cell_grid_pos;
        double to_neighbors_mag_sq = to_neighbors_vec.x * to_neighbors_vec.x + to_neighbors_vec.y * to_neighbors_vec.y;
        Vector2d to_neighbors = to_neighbors_vec * (1.0 / std::sqrt(to_neighbors_mag_sq));

        double alignment = to_neighbors.dot(centering_direction);

        spdlog::trace(
            "Alignment check at ({},{}): to_neighbors=({:.3f},{:.3f}), to_center=({:.3f},{:.3f}), "
            "alignment={:.3f}",
            x,
            y,
            to_neighbors.x,
            to_neighbors.y,
            centering_direction.x,
            centering_direction.y,
            alignment);

        if (alignment > 0.0) {
            // Clustering helps centering → apply it (weighted by alignment strength).
            final_force = final_force + clustering_force * alignment;
            spdlog::trace(
                "Clustering APPLIED (alignment={:.3f}): boost=({:.4f},{:.4f})",
                alignment,
                (clustering_force * alignment).x,
                (clustering_force * alignment).y);
        }
        else {
            spdlog::trace("Clustering SKIPPED (alignment={:.3f} <= 0)", alignment);
        }
    }

    double total_force_magnitude = std::sqrt(final_force.x * final_force.x + final_force.y * final_force.y);

    spdlog::trace(
        "Dual cohesion for {} at ({},{}): connections={}, com_offset={:.3f}, "
        "clustering=({:.3f},{:.3f}), centering=({:.3f},{:.3f}), total_mag={:.3f}",
        getMaterialName(cell.material_type),
        x,
        y,
        connection_count,
        com_offset,
        clustering_force.x,
        clustering_force.y,
        centering_force.x,
        centering_force.y,
        total_force_magnitude);

    return { final_force,
             total_force_magnitude,
             neighbor_center,
             connection_count,
             0.0,
             cell_mass,
             (connection_count > 0 || com_offset_sq > 0.000001) };
}
