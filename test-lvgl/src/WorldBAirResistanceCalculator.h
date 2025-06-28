#pragma once

#include "Vector2d.h"
#include "WorldBCalculatorBase.h"
#include <cstdint>

class CellB;
class WorldB;

/**
 * @brief Calculates air resistance forces for WorldB physics
 *
 * This class implements air resistance (drag) forces that oppose motion.
 * The resistance is proportional to velocity squared and inversely proportional
 * to material density, creating realistic drag effects where:
 * - Faster moving materials experience more resistance
 * - Denser materials are less affected by air resistance
 * - The effect is non-linear (quadratic with velocity)
 */
class WorldBAirResistanceCalculator : public WorldBCalculatorBase {
public:
    /**
     * @brief Constructor takes a WorldB for accessing world data.
     * @param world WorldB providing access to grid and cells.
     */
    explicit WorldBAirResistanceCalculator(const WorldB& world);

    /**
     * @brief Default air resistance scaling factor.
     * Controls the overall strength of air resistance in the simulation.
     * Higher values create more drag, lower values allow freer movement.
     */
    static constexpr double DEFAULT_AIR_RESISTANCE_SCALAR = 0.1;

    /**
     * @brief Calculate air resistance force for a cell.
     * @param x Column coordinate.
     * @param y Row coordinate.
     * @param strength Air resistance strength multiplier (optional, uses default if not provided).
     * @return Air resistance force vector opposing motion.
     */
    Vector2d calculateAirResistance(
        uint32_t x, uint32_t y, double strength = DEFAULT_AIR_RESISTANCE_SCALAR) const;
};