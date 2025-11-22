#pragma once

#include "MaterialType.h"
#include "Vector2d.h"
#include "WorldCalculatorBase.h"
#include <cstdint>
#include <memory>
#include <vector>

namespace DirtSim {

class Cell;
class GridOfCells;
class World;
class WorldSupportCalculator;

/**
 * @brief Calculates cohesion forces for World physics
 *
 * This class encapsulates all cohesion-related calculations including:
 * - Traditional resistance-based cohesion (movement threshold)
 * - Center-of-mass cohesion forces (attractive clustering)
 *
 * The cohesion system implements dual physics:
 * 1. Resistance cohesion: Prevents material movement when neighboring support exists (uses
 * WorldSupportCalculator)
 * 2. Force cohesion: Adds attractive forces toward connected material clusters
 */
class WorldCohesionCalculator : public WorldCalculatorBase {
public:
    WorldCohesionCalculator() = default;

    // Force calculation structures for cohesion physics (moved from World).
    struct CohesionForce {
        double resistance_magnitude;  // Strength of cohesive resistance.
        uint32_t connected_neighbors; // Number of same-material neighbors.
    };

    struct COMCohesionForce {
        Vector2d force_direction;     // Net force direction toward neighbors.
        double force_magnitude;       // Strength of cohesive pull.
        Vector2d center_of_neighbors; // Average position of connected neighbors.
        uint32_t active_connections;  // Number of neighbors contributing.
        // NEW fields for mass-based calculations:
        double total_neighbor_mass; // Sum of all neighbor masses.
        double cell_mass;           // Mass of current cell.
        bool force_active;          // Whether force should be applied (cutoff check).
    };

    // Cohesion-specific constants.
    static constexpr double MIN_SUPPORT_FACTOR = 0.1; // Minimum cohesion when no support.

    /**
     * Prepare the resistance cache for a new frame.
     * Resizes cache if needed and resets all values to uncached sentinel.
     */
    void prepareForFrame(uint32_t width, uint32_t height);

    /**
     * Get cached resistance or calculate it.
     * Returns cached value if available, otherwise calculates and caches it.
     */
    double getCachedOrCalculateResistance(const World& world, uint32_t x, uint32_t y);

    CohesionForce calculateCohesionForce(const World& world, uint32_t x, uint32_t y) const;

    COMCohesionForce calculateCOMCohesionForce(
        const World& world, uint32_t x, uint32_t y, uint32_t com_cohesion_range) const;

private:
    // Frame-scoped cache for resistance values.
    // Index: y * width + x, Value: resistance_magnitude (-1.0 = not cached).
    mutable std::vector<double> resistance_cache_;
    mutable uint32_t cache_width_ = 0;

    uint32_t makeCacheKey(uint32_t x, uint32_t y) const { return y * cache_width_ + x; }

    static constexpr double CACHE_SENTINEL = -1.0; // Sentinel value for uncached.
};

} // namespace DirtSim
