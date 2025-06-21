#include "WorldBAirResistanceCalculator.h"
#include "CellB.h"
#include "MaterialType.h"
#include "WorldB.h"
#include <cmath>
#include <spdlog/spdlog.h>

WorldBAirResistanceCalculator::WorldBAirResistanceCalculator(const WorldB& world)
    : WorldBCalculatorBase(world)
{}

Vector2d WorldBAirResistanceCalculator::calculateAirResistance(
    uint32_t x, uint32_t y, double strength) const
{
    const CellB& cell = getCellAt(x, y);

    // No air resistance for empty or wall cells
    if (cell.isEmpty() || cell.isWall()) {
        return Vector2d(0.0, 0.0);
    }

    // Get cell velocity
    Vector2d velocity = cell.getVelocity();
    double velocity_magnitude = velocity.mag();

    // No resistance if not moving
    if (velocity_magnitude < MIN_MATTER_THRESHOLD) {
        return Vector2d(0.0, 0.0);
    }

    // Get material properties
    MaterialType material = cell.getMaterialType();
    double density = getMaterialDensity(material);

    // Calculate air resistance force
    // F = -k * v² * (1/density) * v_hat
    // Where:
    // - k is the air resistance scalar
    // - v² creates non-linear (quadratic) relationship with velocity
    // - 1/density makes denser materials less affected
    // - v_hat is the unit vector in velocity direction (to oppose motion)

    Vector2d velocity_direction = velocity.normalize();
    double density_factor = 1.0 / density; // Inverse relationship with density
    double force_magnitude = strength * velocity_magnitude * velocity_magnitude * density_factor;

    // Force opposes motion (negative of velocity direction)
    Vector2d air_resistance_force = velocity_direction * (-force_magnitude);

    // Debug logging for significant forces
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