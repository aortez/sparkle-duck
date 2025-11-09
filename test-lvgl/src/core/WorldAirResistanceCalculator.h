#pragma once

#include "Vector2d.h"
#include "WorldCalculatorBase.h"
#include <cstdint>

namespace DirtSim {

class Cell;
class World;

/**
 * @brief Calculates air resistance forces for World physics
 *
 * This class implements air resistance (drag) forces that oppose motion.
 * The drag force is proportional to velocity squared (F = k*v²), creating
 * realistic quadratic drag behavior where:
 * - Faster moving materials experience quadratically more resistance
 * - All materials experience the same drag force at the same velocity
 * - Denser materials are naturally less affected during integration (a = F/m)
 * - The effect is non-linear (quadratic with velocity)
 */
class WorldAirResistanceCalculator : public WorldCalculatorBase {
public:
    // Default constructor - calculator is stateless.
    WorldAirResistanceCalculator() = default;

    /**
     * @brief Default air resistance scaling factor.
     * Controls the overall strength of air resistance in the simulation.
     * Higher values create more drag, lower values allow freer movement.
     */
    static constexpr double DEFAULT_AIR_RESISTANCE_SCALAR = 0.1;

    /**
     * @brief Calculate air resistance force for a cell.
     * @param world World providing access to grid and cells.
     * @param x Column coordinate.
     * @param y Row coordinate.
     * @param strength Air resistance strength multiplier (optional, uses default if not provided).
     * @return Air resistance force vector opposing motion.
     */
    Vector2d calculateAirResistance(
        const World& world,
        uint32_t x,
        uint32_t y,
        double strength = DEFAULT_AIR_RESISTANCE_SCALAR) const;
};

} // namespace DirtSim