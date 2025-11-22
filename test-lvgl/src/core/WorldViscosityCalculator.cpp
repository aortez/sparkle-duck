#include "WorldViscosityCalculator.h"
#include "Cell.h"
#include "World.h"
#include <cmath>
#include <spdlog/spdlog.h>

namespace DirtSim {

Vector2d WorldViscosityCalculator::calculateNeighborVelocityAverage(
    const World& world, uint32_t x, uint32_t y, MaterialType centerMaterial) const
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
            if (nx < 0 || ny < 0 || nx >= static_cast<int>(world.data.width)
                || ny >= static_cast<int>(world.data.height)) {
                continue;
            }

            const Cell& neighbor = world.at(static_cast<uint32_t>(nx), static_cast<uint32_t>(ny));

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
    const World& world, uint32_t x, uint32_t y) const
{
    const Cell& cell = world.at(x, y);

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
        calculateNeighborVelocityAverage(world, x, y, cell.material_type);

    // Velocity difference drives viscous force.
    Vector2d velocity_difference = avg_neighbor_velocity - cell.velocity;

    // Check if support comes from solid material (not fluid).
    bool has_solid_support = false;
    if (cell.has_any_support && y < world.data.height - 1) {
        const Cell& below = world.at(x, y + 1);
        if (!below.isEmpty()) {
            const MaterialProperties& below_props = getMaterialProperties(below.material_type);
            has_solid_support = !below_props.is_fluid;
        }
    }
    double support_factor = has_solid_support ? 1.0 : 0.0;

    // Determine motion state (STATIC when supported, FALLING otherwise).
    // TODO: Implement full motion state detection (SLIDING, TURBULENT).
    World::MotionState motion_state =
        support_factor > 0.5 ? World::MotionState::STATIC : World::MotionState::FALLING;

    // Calculate motion state multiplier.
    double motion_multiplier =
        world.getMotionStateMultiplier(motion_state, props.motion_sensitivity);

    // Calculate effective viscosity with motion state and support modulation.
    // Support increases coupling strength (more contact = more shear).
    double effective_viscosity = props.viscosity * motion_multiplier * (1.0 + support_factor);

    // Viscous force tries to eliminate velocity differences.
    // Scale by viscosity strength (UI control) and fill ratio.
    Vector2d viscous_force = velocity_difference * effective_viscosity
        * world.physicsSettings.viscosity_strength * cell.fill_ratio;

    // Debug info.
    double neighbor_avg_speed = avg_neighbor_velocity.magnitude();
    int neighbor_count = 0; // TODO: Track actual count if needed for debugging.

    return ViscousForce{ .force = viscous_force,
                         .neighbor_avg_speed = neighbor_avg_speed,
                         .neighbor_count = neighbor_count };
}

} // namespace DirtSim
