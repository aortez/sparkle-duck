#ifndef WORLDBADHESIONCALCULATOR_H
#define WORLDBADHESIONCALCULATOR_H

#include "MaterialType.h"
#include "Vector2d.h"
#include "WorldCalculatorBase.h"
#include "bitmaps/MaterialNeighborhood.h"

namespace DirtSim {

class World;

/**
 * Calculator for adhesion forces between cells in World.
 *
 * Adhesion forces create attractive forces between neighboring cells of
 * different material types. The force strength is based on the geometric
 * mean of the materials' adhesion properties, weighted by fill ratios
 * and distance.
 */
class WorldAdhesionCalculator : public WorldCalculatorBase {
public:
    // Data structure for adhesion force results.
    struct AdhesionForce {
        Vector2d force_direction;     // Direction of adhesive pull/resistance.
        double force_magnitude;       // Strength of adhesive force.
        MaterialType target_material; // Strongest interacting material.
        uint32_t contact_points;      // Number of contact interfaces.
    };

    // Default constructor - calculator is stateless.
    WorldAdhesionCalculator() = default;

    // Main calculation method.
    AdhesionForce calculateAdhesionForce(const World& world, uint32_t x, uint32_t y) const;

    // Cache-optimized version using MaterialNeighborhood.
    AdhesionForce calculateAdhesionForce(
        const World& world, uint32_t x, uint32_t y, const MaterialNeighborhood& mat_n) const;

    // Adhesion parameters - NOTE: Now uses World.physicsSettings, these are legacy wrappers.
    // These methods are kept for backward compatibility but delegate to World.physicsSettings.
};

} // namespace DirtSim

#endif // WORLDBADHESIONCALCULATOR_H