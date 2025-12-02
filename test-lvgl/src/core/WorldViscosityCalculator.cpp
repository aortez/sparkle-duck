#include "WorldViscosityCalculator.h"
#include "Cell.h"
#include "GridOfCells.h"
#include "PhysicsSettings.h"
#include "World.h"
#include "WorldData.h"
#include <cmath>
#include <spdlog/spdlog.h>

namespace DirtSim {

Vector2d WorldViscosityCalculator::calculateNeighborVelocityAverage(
    const WorldData& data, uint32_t x, uint32_t y, MaterialType centerMaterial) const
{
    Vector2d velocity_sum{ 0.0, 0.0 };
    double weight_sum = 0.0;

    // Check all 8 neighbors.
    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            if (dx == 0 && dy == 0) {
                continue; // Skip self.
            }

            int nx = static_cast<int>(x) + dx;
            int ny = static_cast<int>(y) + dy;

            // Bounds check.
            if (nx < 0 || ny < 0 || nx >= static_cast<int>(data.width)
                || ny >= static_cast<int>(data.height)) {
                continue;
            }

            const Cell& neighbor = data.at(static_cast<uint32_t>(nx), static_cast<uint32_t>(ny));

            // Only couple with same-material neighbors.
            if (neighbor.material_type != centerMaterial || neighbor.isEmpty()) {
                continue;
            }

            // Distance weighting (diagonal neighbors are farther).
            double distance_weight = (dx != 0 && dy != 0) ? 0.707 : 1.0;

            // Fill ratio weighting (more matter = stronger influence).
            double fill_weight = neighbor.fill_ratio;

            // Combined weight.
            double weight = distance_weight * fill_weight;

            velocity_sum += neighbor.velocity * weight;
            weight_sum += weight;
        }
    }

    // Return weighted average, or zero if no neighbors.
    if (weight_sum > 0.0) {
        return velocity_sum / weight_sum;
    }
    return Vector2d{ 0.0, 0.0 };
}

WorldViscosityCalculator::ViscousForce WorldViscosityCalculator::calculateViscousForce(
    const World& world,
    uint32_t x,
    uint32_t y,
    double viscosity_strength,
    const GridOfCells* grid) const
{
    (void)grid; // Unused: now using cell.has_any_support instead of grid->supportBitmap().

    // Cache data reference to avoid repeated pImpl dereferences.
    const WorldData& data = world.getData();
    const Cell& cell = data.at(x, y);

    // Skip empty cells and walls.
    if (cell.isEmpty() || cell.isWall()) {
        return ViscousForce{ .force = { 0.0, 0.0 },
                             .neighbor_avg_speed = 0.0,
                             .neighbor_count = 0 };
    }

    // Get material properties.
    const MaterialProperties& props = getMaterialProperties(cell.material_type);

    // Skip if viscosity is zero.
    if (props.viscosity <= 0.0) {
        return ViscousForce{ .force = { 0.0, 0.0 },
                             .neighbor_avg_speed = 0.0,
                             .neighbor_count = 0 };
    }

    // Calculate weighted average velocity of same-material neighbors.
    Vector2d avg_neighbor_velocity =
        calculateNeighborVelocityAverage(data, x, y, cell.material_type);

    // Velocity difference drives viscous force.
    Vector2d velocity_difference = avg_neighbor_velocity - cell.velocity;

    // Check for solid support.
    // Assert that support bitmap matches cell field (debug/verify consistency).
    if (grid && GridOfCells::USE_CACHE) {
        bool bitmap_support = grid->supportBitmap().isSet(x, y);
        bool cell_support = cell.has_any_support;

        if (bitmap_support != cell_support) {
            spdlog::error(
                "SUPPORT MISMATCH at [{},{}]: bitmap={}, cell.has_any_support={}",
                x,
                y,
                bitmap_support,
                cell_support);
            spdlog::error(
                "  Cell: material={}, fill={:.2f}, has_vertical={}, has_any={}",
                static_cast<int>(cell.material_type),
                cell.fill_ratio,
                cell.has_vertical_support,
                cell.has_any_support);
            // Abort to catch this immediately during testing.
            std::abort();
        }
    }

    // EXPERIMENT: Simplified viscosity - no motion state or support modulation.
    // Just use base material viscosity to test if physics work without support concept.
    double effective_viscosity = props.viscosity;

    // Viscous force tries to eliminate velocity differences.
    // Scale by viscosity strength (UI control) and fill ratio.
    Vector2d viscous_force =
        velocity_difference * effective_viscosity * viscosity_strength * cell.fill_ratio;

    // Debug info.
    double neighbor_avg_speed = avg_neighbor_velocity.magnitude();
    int neighbor_count = 0; // TODO: Track actual count if needed for debugging.

    return ViscousForce{ .force = viscous_force,
                         .neighbor_avg_speed = neighbor_avg_speed,
                         .neighbor_count = neighbor_count };
}

} // namespace DirtSim
