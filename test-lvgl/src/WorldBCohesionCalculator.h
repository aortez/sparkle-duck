#pragma once

#include "WorldBCalculatorBase.h"
#include "MaterialType.h"
#include "Vector2d.h"
#include <cstdint>
#include <memory>

class CellB;
class WorldB;
class WorldBSupportCalculator;

/**
 * @brief Calculates cohesion forces for WorldB physics
 *
 * This class encapsulates all cohesion-related calculations including:
 * - Traditional resistance-based cohesion (movement threshold)
 * - Center-of-mass cohesion forces (attractive clustering)
 *
 * The cohesion system implements dual physics:
 * 1. Resistance cohesion: Prevents material movement when neighboring support exists (uses
 * WorldBSupportCalculator)
 * 2. Force cohesion: Adds attractive forces toward connected material clusters
 */
class WorldBCohesionCalculator : public WorldBCalculatorBase {
public:
    /**
     * @brief Constructor takes a WorldB for accessing world data
     * @param world WorldB providing access to grid and cells
     */
    explicit WorldBCohesionCalculator(const WorldB& world);

    // Force calculation structures for cohesion physics (moved from WorldB)
    struct CohesionForce {
        double resistance_magnitude;  // Strength of cohesive resistance
        uint32_t connected_neighbors; // Number of same-material neighbors
    };

    struct COMCohesionForce {
        Vector2d force_direction;     // Net force direction toward neighbors
        double force_magnitude;       // Strength of cohesive pull
        Vector2d center_of_neighbors; // Average position of connected neighbors
        uint32_t active_connections;  // Number of neighbors contributing
        // NEW fields for mass-based calculations:
        double total_neighbor_mass; // Sum of all neighbor masses
        double cell_mass;           // Mass of current cell
        bool force_active;          // Whether force should be applied (cutoff check)
    };

    // Cohesion-specific constants
    static constexpr double MIN_SUPPORT_FACTOR = 0.1; // Minimum cohesion when no support

    CohesionForce calculateCohesionForce(uint32_t x, uint32_t y) const;

    COMCohesionForce calculateCOMCohesionForce(
        uint32_t x, uint32_t y, uint32_t com_cohesion_range) const;

private:
    mutable std::unique_ptr<WorldBSupportCalculator> support_calculator_; // Support calculations
};
