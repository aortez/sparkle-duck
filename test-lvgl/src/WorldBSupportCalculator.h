#pragma once

#include "WorldBCalculatorBase.h"
#include "MaterialType.h"
#include "Vector2d.h"
#include <cstdint>

// Forward declarations
class CellB;
class WorldB;

/**
 * @brief Calculates structural support for WorldB physics
 *
 * This class encapsulates all structural support calculations including:
 * - Vertical support analysis (continuous material to ground)
 * - Horizontal support analysis (rigid lateral connections)
 * - Distance to support calculations for cohesion decay
 * - Overall structural support determination
 *
 * The support system implements realistic physics where materials
 * require structural foundation (vertical) or rigid connections (horizontal)
 * to maintain cohesion and resist movement.
 */
class WorldBSupportCalculator : public WorldBCalculatorBase {
public:
    /**
     * @brief Constructor takes a WorldB for accessing world data
     * @param world WorldB providing access to grid and cells
     */
    explicit WorldBSupportCalculator(const WorldB& world);

    // Support-specific constants
    static constexpr uint32_t MAX_VERTICAL_SUPPORT_DISTANCE =
        5;                                                   // Max distance for vertical support
    static constexpr double RIGID_DENSITY_THRESHOLD = 5.0;   // Density threshold for rigid support
    static constexpr double STRONG_ADHESION_THRESHOLD = 0.5; // Min adhesion for horizontal support
    static constexpr uint32_t MAX_SUPPORT_DISTANCE = 10;     // Max distance for any support search

    /**
     * @brief Check if cell has vertical structural support
     *
     * Determines if a cell has vertical support by checking for continuous material
     * below up to MAX_VERTICAL_SUPPORT_DISTANCE with recursive validation.
     *
     * @param x Cell X coordinate
     * @param y Cell Y coordinate
     * @return true if cell has vertical support
     */
    bool hasVerticalSupport(uint32_t x, uint32_t y) const;

    /**
     * @brief Check if cell has horizontal structural support
     *
     * Determines if a cell has horizontal support by checking immediate neighbors
     * for rigid materials with strong mutual adhesion.
     *
     * @param x Cell X coordinate
     * @param y Cell Y coordinate
     * @return true if cell has horizontal support
     */
    bool hasHorizontalSupport(uint32_t x, uint32_t y) const;

    /**
     * @brief Check if a position has structural support
     *
     * Determines overall structural support by checking both vertical and horizontal
     * support systems. Used for determining material stability.
     *
     * @param x Cell X coordinate
     * @param y Cell Y coordinate
     * @return true if cell has any form of structural support
     */
    bool hasStructuralSupport(uint32_t x, uint32_t y) const;

    /**
     * @brief Calculate distance to structural support
     *
     * Calculates the minimum distance to any form of structural support,
     * used for cohesion decay calculations. Implements BFS to find nearest
     * supported material.
     *
     * @param x Cell X coordinate
     * @param y Cell Y coordinate
     * @return Distance to nearest structural support (higher = less stable)
     */
    double calculateDistanceToSupport(uint32_t x, uint32_t y) const;

private:
    // No additional private members needed
};