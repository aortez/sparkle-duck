#ifndef WORLDBADHESIONCALCULATOR_H
#define WORLDBADHESIONCALCULATOR_H

#include "WorldBCalculatorBase.h"
#include "Vector2d.h"
#include "MaterialType.h"

class WorldB;

/**
 * Calculator for adhesion forces between cells in WorldB.
 * 
 * Adhesion forces create attractive forces between neighboring cells of
 * different material types. The force strength is based on the geometric
 * mean of the materials' adhesion properties, weighted by fill ratios
 * and distance.
 */
class WorldBAdhesionCalculator : public WorldBCalculatorBase {
public:
    // Data structure for adhesion force results
    struct AdhesionForce {
        Vector2d force_direction;     // Direction of adhesive pull/resistance
        double force_magnitude;       // Strength of adhesive force
        MaterialType target_material; // Strongest interacting material
        uint32_t contact_points;      // Number of contact interfaces
    };

    // Constructor
    explicit WorldBAdhesionCalculator(const WorldB& world);

    // Main calculation method
    AdhesionForce calculateAdhesionForce(uint32_t x, uint32_t y) const;

    // Adhesion parameters
    void setAdhesionEnabled(bool enabled) { adhesion_enabled_ = enabled; }
    bool isAdhesionEnabled() const { return adhesion_enabled_; }
    
    void setAdhesionStrength(double strength) { adhesion_strength_ = strength; }
    double getAdhesionStrength() const { return adhesion_strength_; }

private:
    // Configuration parameters
    bool adhesion_enabled_ = true;
    double adhesion_strength_ = 5.0;
};

#endif // WORLDBADHESIONCALCULATOR_H