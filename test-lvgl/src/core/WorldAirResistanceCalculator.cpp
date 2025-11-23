#include "WorldAirResistanceCalculator.h"
#include "Cell.h"
#include "MaterialType.h"
#include "PhysicsSettings.h"
#include "World.h"
#include "WorldData.h"
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

    // Get material properties.
    MaterialType material = cell.material_type;
    const MaterialProperties& props = getMaterialProperties(material);

    // Calculate air resistance force using proper physics.
    // F_drag = -k * v² * v̂
    // Where:
    // - k is the air resistance coefficient (material-specific drag).
    // - v² creates realistic quadratic drag relationship.
    // - v̂ is the unit vector opposing motion.
    //
    // Material-specific air resistance models shape, surface area, and density effects.

    Vector2d velocity_direction = velocity.normalize();
    double force_magnitude =
        strength * props.air_resistance * velocity_magnitude * velocity_magnitude;

    // Force opposes motion (negative of velocity direction).
    Vector2d air_resistance_force = velocity_direction * (-force_magnitude);

    // Debug logging for significant forces.
    if (force_magnitude > 0.01) {
        spdlog::trace(
            "Air resistance at ({},{}) {}: velocity=({:.3f},{:.3f}), "
            "magnitude={:.3f}, air_resist={:.2f}, force=({:.3f},{:.3f})",
            x,
            y,
            getMaterialName(material),
            velocity.x,
            velocity.y,
            velocity_magnitude,
            props.air_resistance,
            air_resistance_force.x,
            air_resistance_force.y);
    }

    return air_resistance_force;
}