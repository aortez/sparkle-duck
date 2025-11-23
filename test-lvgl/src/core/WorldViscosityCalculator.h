#ifndef WORLDVISCOSITYCALCULATOR_H
#define WORLDVISCOSITYCALCULATOR_H

#include "MaterialType.h"
#include "Vector2d.h"
#include "WorldCalculatorBase.h"

namespace DirtSim {

class GridOfCells;
class World;
struct WorldData;

/**
 * Calculator for viscous forces between cells in World.
 *
 * Implements viscosity as momentum diffusion - velocities of adjacent
 * same-material cells are averaged to create shear forces. This causes
 * velocity fields to smooth out over time, with high-viscosity materials
 * resisting velocity gradients more strongly.
 *
 * Key features:
 * - Same-material coupling only (water doesn't drag dirt)
 * - Distance-weighted neighbors (diagonal × 0.707)
 * - Fill ratio weighting (more matter = stronger coupling)
 * - Support factor amplification (supported materials couple more)
 * - Motion state integration (STATIC vs FALLING affects coupling)
 */
class WorldViscosityCalculator : public WorldCalculatorBase {
public:
    // Data structure for viscous force results.
    struct ViscousForce {
        Vector2d force;            // Net viscous force from all neighbors.
        double neighbor_avg_speed; // Average speed of same-material neighbors.
        int neighbor_count;        // Number of neighbors used in average.
    };

    // Default constructor - calculator is stateless.
    WorldViscosityCalculator() = default;

    /**
     * Calculate viscous force for a cell based on velocity differences with neighbors.
     * @param world The world containing the cell.
     * @param x Cell x coordinate.
     * @param y Cell y coordinate.
     * @return Viscous force structure with force vector and debug info.
     */
    ViscousForce calculateViscousForce(
        const World& world,
        uint32_t x,
        uint32_t y,
        double viscosity_strength,
        const GridOfCells* grid = nullptr) const;

private:
    /**
     * Calculate weighted average velocity of same-material neighbors.
     * @param world The world containing the cell.
     * @param x Cell x coordinate.
     * @param y Cell y coordinate.
     * @param centerMaterial Material type to match.
     * @return Weighted average velocity of same-material neighbors.
     */
    Vector2d calculateNeighborVelocityAverage(
        const WorldData& data, uint32_t x, uint32_t y, MaterialType centerMaterial) const;
};

} // namespace DirtSim

#endif // WORLDVISCOSITYCALCULATOR_H
