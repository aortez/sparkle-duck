#pragma once

#include "MaterialType.h"
#include "Vector2d.h"
#include "WorldCalculatorBase.h"
#include <cstdint>

namespace DirtSim {

// Forward declarations
class Cell;
class EmptyNeighborhood;
class GridOfCells;
class MaterialNeighborhood;
class World;
class GridOfCells;

/**
 * @brief Calculates structural support for World physics
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
class WorldSupportCalculator : public WorldCalculatorBase {
public:
    // Constructor requires GridOfCells reference for bitmap and cell access.
    explicit WorldSupportCalculator(GridOfCells& grid);

    // Support-specific constants.
    static constexpr uint32_t MAX_VERTICAL_SUPPORT_DISTANCE =
        5; // Max distance for vertical support.
    static constexpr double COHESION_SUPPORT_THRESHOLD =
        0.5; // Min cohesion for same-material horizontal support.
    static constexpr double ADHESION_SUPPORT_THRESHOLD =
        0.5; // Min adhesion for different-material horizontal support.
    static constexpr uint32_t MAX_SUPPORT_DISTANCE = 10; // Max distance for any support search.

    /**
     * @brief Check if cell has vertical structural support.
     *
     * Determines if a cell has vertical support by checking for continuous material
     * below up to MAX_VERTICAL_SUPPORT_DISTANCE with recursive validation.
     *
     * @param world World providing access to grid and cells.
     * @param x Cell X coordinate.
     * @param y Cell Y coordinate.
     * @return true if cell has vertical support.
     */
    bool hasVerticalSupport(const World& world, uint32_t x, uint32_t y) const;

    /**
     * @brief Check if cell has horizontal structural support.
     *
     * Determines if a cell has horizontal support by checking immediate neighbors
     * for rigid materials with strong mutual adhesion.
     *
     * @param world World providing access to grid and cells.
     * @param x Cell X coordinate.
     * @param y Cell Y coordinate.
     * @return true if cell has horizontal support.
     */
    bool hasHorizontalSupport(const World& world, uint32_t x, uint32_t y) const;

    bool hasHorizontalSupport(
        uint32_t x,
        uint32_t y,
        const EmptyNeighborhood& empty_n,
        const MaterialNeighborhood& mat_n) const;

    /**
     * @brief Check if a position has structural support.
     *
     * Determines overall structural support by checking both vertical and horizontal
     * support systems. Used for determining material stability.
     *
     * @param x Cell X coordinate.
     * @param y Cell Y coordinate.
     * @return true if cell has any form of structural support.
     */
    bool hasStructuralSupport(uint32_t x, uint32_t y) const;

    /**
     * @brief Compute support map for entire grid using bottom-up scan.
     *
     * Calculates support for all cells in a single bottom-to-top pass.
     * Much more efficient than per-cell recursive checks.
     *
     * Uses GridOfCells::USE_CACHE to toggle between bitmap lookups and direct cell access.
     *
     * @param world World to compute support for (modifies cached_has_support).
     */
    void computeSupportMapBottomUp(World& world) const;

    /**
     * @brief Calculate distance to structural support.
     *
     * Calculates the minimum distance to any form of structural support,
     * used for cohesion decay calculations. Implements BFS to find nearest
     * supported material.
     *
     * @param world World providing access to grid and cells.
     * @param x Cell X coordinate.
     * @param y Cell Y coordinate.
     * @return Distance to nearest structural support (higher = less stable).
     */
    double calculateDistanceToSupport(const World& world, uint32_t x, uint32_t y) const;

private:
    GridOfCells& grid_; // Required reference to grid for bitmap and cell access.
};

} // namespace DirtSim
