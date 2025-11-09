#include "WorldAirResistanceCalculator.h"
#include "Cell.h"
#include "MaterialType.h"
#include "World.h"
#include <cmath>
#include <spdlog/spdlog.h>

using namespace DirtSim;

Vector2d WorldAirResistanceCalculator::calculateAirResistance(
    const World& world, uint32_t x, uint32_t y, double strength) const
{
    const Cell& cell = getCellAt(world, x, y);

    // No air resistance for empty or wall cells.
    if (cell.isEmpty() || cell.isWall()) {
        return Vector2d{ 0.0, 0.0 };
    }

    // Get cell velocity.
    Vector2d velocity = cell.velocity;
    double velocity_magnitude = velocity.mag();

    // No resistance if not moving.
    if (velocity_magnitude < MIN_MATTER_THRESHOLD) {
        return Vector2d{ 0.0, 0.0 };
    }

    // Get material properties for logging.
    MaterialType material = cell.material_type;
    double density = getMaterialDensity(material);

    // Calculate air resistance force using proper physics.
    // F_drag = -k * v² * v̂
    // Where:
    // - k is the air resistance coefficient (depends on cross-section, shape, air density).
    // - v² creates realistic quadratic drag relationship.
    // - v̂ is the unit vector opposing motion.
    //
    // Dense materials are naturally less affected because a = F/m during integration.

    Vector2d velocity_direction = velocity.normalize();
    double force_magnitude = strength * velocity_magnitude * velocity_magnitude;

    // Force opposes motion (negative of velocity direction).
    Vector2d air_resistance_force = velocity_direction * (-force_magnitude);

    // Debug logging for significant forces.
    if (force_magnitude > 0.01) {
        spdlog::trace(
            "Air resistance at ({},{}) {}: velocity=({:.3f},{:.3f}), "
            "magnitude={:.3f}, density={:.1f}, force=({:.3f},{:.3f})",
            x,
            y,
            getMaterialName(material),
            velocity.x,
            velocity.y,
            velocity_magnitude,
            density,
            air_resistance_force.x,
            air_resistance_force.y);
    }

    return air_resistance_force;
}