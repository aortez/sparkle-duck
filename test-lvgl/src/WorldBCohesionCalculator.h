#pragma once

#include "Vector2d.h"
#include "MaterialType.h"
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
 * 1. Resistance cohesion: Prevents material movement when neighboring support exists (uses WorldBSupportCalculator)
 * 2. Force cohesion: Adds attractive forces toward connected material clusters
 */
class WorldBCohesionCalculator {
public:
    /**
     * @brief Constructor takes a WorldB for accessing world data
     * @param world WorldB providing access to grid and cells
     */
    explicit WorldBCohesionCalculator(const WorldB& world);
    
    // Disable copy construction and assignment
    WorldBCohesionCalculator(const WorldBCohesionCalculator&) = delete;
    WorldBCohesionCalculator& operator=(const WorldBCohesionCalculator&) = delete;
    
    // Allow move construction and assignment
    WorldBCohesionCalculator(WorldBCohesionCalculator&&) = default;
    WorldBCohesionCalculator& operator=(WorldBCohesionCalculator&&) = default;

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
    };
    
    // Cohesion calculation constants
    static constexpr double MIN_MATTER_THRESHOLD = 0.001;      // Minimum matter to process
    static constexpr double MIN_SUPPORT_FACTOR = 0.1;          // Minimum cohesion when no support

    CohesionForce calculateCohesionForce(uint32_t x, uint32_t y) const;
    
    COMCohesionForce calculateCOMCohesionForce(uint32_t x, uint32_t y, uint32_t com_cohesion_range) const;


private:
    const WorldB& world_;
    mutable std::unique_ptr<WorldBSupportCalculator> support_calculator_; // Support calculations
    
    const CellB& getCellAt(uint32_t x, uint32_t y) const;
    
    bool isValidCell(int x, int y) const;
};
