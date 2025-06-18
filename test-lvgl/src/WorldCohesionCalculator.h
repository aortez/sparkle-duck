#pragma once

#include "Vector2d.h"
#include "MaterialType.h"
#include <cstdint>

// Forward declarations
class CellB;
class WorldInterface;

/**
 * @brief Calculates cohesion forces and structural support for WorldB physics
 * 
 * This class encapsulates all cohesion-related calculations including:
 * - Traditional resistance-based cohesion (movement threshold)
 * - Center-of-mass cohesion forces (attractive clustering)
 * - Structural support analysis (vertical/horizontal)
 * 
 * The cohesion system implements dual physics:
 * 1. Resistance cohesion: Prevents material movement when neighboring support exists
 * 2. Force cohesion: Adds attractive forces toward connected material clusters
 */
class WorldCohesionCalculator {
public:
    /**
     * @brief Constructor takes a WorldInterface for accessing world data
     * @param world WorldInterface providing access to grid and cells
     */
    explicit WorldCohesionCalculator(const WorldInterface& world);
    
    // Disable copy construction and assignment
    WorldCohesionCalculator(const WorldCohesionCalculator&) = delete;
    WorldCohesionCalculator& operator=(const WorldCohesionCalculator&) = delete;
    
    // Allow move construction and assignment
    WorldCohesionCalculator(WorldCohesionCalculator&&) = default;
    WorldCohesionCalculator& operator=(WorldCohesionCalculator&&) = default;
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
    static constexpr uint32_t MAX_VERTICAL_SUPPORT_DISTANCE = 5; // Max distance for vertical support
    static constexpr double RIGID_DENSITY_THRESHOLD = 5.0;     // Density threshold for rigid support
    static constexpr double STRONG_ADHESION_THRESHOLD = 0.5;   // Min adhesion for horizontal support
    static constexpr uint32_t MAX_SUPPORT_DISTANCE = 10;       // Max distance for any support search

    /**
     * @brief Calculate resistance-based cohesion force
     * 
     * Calculates cohesion resistance based on same-material neighbors and structural
     * support. Uses directional support analysis to determine resistance magnitude.
     * 
     * @param x Cell X coordinate
     * @param y Cell Y coordinate
     * @return CohesionForce with resistance magnitude and neighbor count
     */
    CohesionForce calculateCohesionForce(uint32_t x, uint32_t y) const;
    
    /**
     * @brief Calculate center-of-mass cohesion force
     * 
     * Calculates COM-based cohesion force that pulls particles toward the weighted
     * center of connected neighbors within the specified range.
     * 
     * @param x Cell X coordinate
     * @param y Cell Y coordinate
     * @param com_cohesion_range Range for neighbor detection
     * @return COMCohesionForce with direction, magnitude, and connection data
     */
    COMCohesionForce calculateCOMCohesionForce(uint32_t x, uint32_t y, uint32_t com_cohesion_range) const;

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

private:
    // Reference to the world for accessing grid data
    const WorldInterface& world_;
    
    /**
     * @brief Get cell at specific coordinates
     * @param x Column coordinate
     * @param y Row coordinate
     * @return Reference to cell at (x,y)
     */
    const CellB& getCellAt(uint32_t x, uint32_t y) const;
    
    /**
     * @brief Check if coordinates are valid for the grid
     * @param x Column coordinate
     * @param y Row coordinate
     * @return true if coordinates are within bounds
     */
    bool isValidCell(int x, int y) const;
};