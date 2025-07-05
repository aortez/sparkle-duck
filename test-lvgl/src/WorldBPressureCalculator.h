#pragma once

#include "MaterialMove.h"
#include "MaterialType.h"
#include "Vector2d.h"
#include "WorldBCalculatorBase.h"
#include "WorldInterface.h"

#include <cstdint>
#include <vector>

class CellB;
class WorldB;

/**
 * @brief Calculates pressure forces for WorldB physics.
 *
 * See GridMechanics.md for more info.
 *
 */
class WorldBPressureCalculator : public WorldBCalculatorBase {
public:
    /**
     * @brief Constructor takes a WorldB for accessing and modifying world data.
     * @param world WorldB providing access to grid and cells (non-const for modifications).
     */
    explicit WorldBPressureCalculator(WorldB& world);

    // Blocked transfer data for dynamic pressure accumulation.
    struct BlockedTransfer {
        int fromX, fromY;       // Source cell coordinates.
        int toX, toY;           // Target cell coordinates.
        double transfer_amount; // Amount that was blocked.
        Vector2d velocity;      // Velocity at time of blocking.
        double energy;          // Kinetic energy of blocked transfer.
    };

    // Pressure-specific constants.
    static constexpr double SLICE_THICKNESS = 1.0;          // Thickness of pressure slices.
    static constexpr double HYDROSTATIC_MULTIPLIER = 0.002; // Hydrostatic force strength.
    static constexpr double DYNAMIC_MULTIPLIER = 1;      // Dynamic force strength.
    static constexpr double DYNAMIC_DECAY_RATE = 0.02;      // Rate of pressure dissipation.
    static constexpr double MIN_PRESSURE_THRESHOLD = 0.001;  // Ignore pressures below this.


    /**
     * @brief Calculate hydrostatic pressure for all cells.
     *
     * Implements slice-based calculation perpendicular to gravity direction.
     * Pressure accumulates based on material density and gravity magnitude.
     */
    void calculateHydrostaticPressure();

    /**
     * @brief Queue a blocked transfer for dynamic pressure accumulation.
     * @param transfer BlockedTransfer data including source, target, and energy.
     */
    void queueBlockedTransfer(const BlockedTransfer& transfer);

    /**
     * @brief Process blocked transfers and accumulate dynamic pressure.
     * @param blocked_transfers Vector of blocked transfers to process.
     *
     * Converts blocked kinetic energy into dynamic pressure at source cells.
     * Updates pressure gradients based on blocked transfer directions.
     */
    void processBlockedTransfers(const std::vector<BlockedTransfer>& blocked_transfers);



    /**
     * @brief Get material-specific hydrostatic pressure sensitivity.
     * @param type Material type.
     * @return Weight factor for hydrostatic pressure [0,1].
     */
    double getHydrostaticWeight(MaterialType type) const;

    /**
     * @brief Get material-specific dynamic pressure sensitivity.
     * @param type Material type.
     * @return Weight factor for dynamic pressure [0,1].
     */
    double getDynamicWeight(MaterialType type) const;

    /**
     * @brief Calculate pressure gradient at a cell position.
     * @param x X coordinate of cell.
     * @param y Y coordinate of cell.
     * @return Pressure gradient vector pointing from high to low pressure.
     *
     * Calculates the pressure gradient by comparing total pressure (hydrostatic + dynamic)
     * with neighboring cells. The gradient points in the direction of decreasing pressure.
     */
    Vector2d calculatePressureGradient(uint32_t x, uint32_t y) const;
    
    /**
     * @brief Calculate expected gravity gradient at a cell position.
     * @param x X coordinate of cell.
     * @param y Y coordinate of cell.
     * @return Gravity gradient vector representing expected equilibrium pressure gradient.
     *
     * Calculates the expected pressure gradient due to gravity based on material density
     * differences with neighbors. In equilibrium, this should balance the pressure gradient.
     */
    Vector2d calculateGravityGradient(uint32_t x, uint32_t y) const;



    /**
     * @brief Generate virtual gravity transfers for pressure accumulation.
     * @param deltaTime Time step for the current frame.
     *
     * Creates virtual blocked transfers from gravity forces acting on material.
     * Even when material is at rest, gravity is always trying to pull it down.
     * If the downward path is blocked, this gravitational force converts to pressure.
     * This allows dynamic pressure to naturally model hydrostatic-like behavior.
     */
    void generateVirtualGravityTransfers(double deltaTime);

    /**
     * @brief Apply pressure decay to dynamic pressure values.
     * @param deltaTime Time step for the current frame.
     *
     * Decays dynamic pressure over time. Hydrostatic pressure does not decay.
     * This should be called after material moves are complete.
     */
    void applyPressureDecay(double deltaTime);

    /**
     * @brief Apply pressure diffusion between neighboring cells.
     * @param deltaTime Time step for the current frame.
     *
     * Implements material-specific pressure propagation using 4-neighbor diffusion.
     * Pressure spreads from high to low pressure regions based on material
     * diffusion coefficients. Walls act as barriers with zero flux.
     */
    void applyPressureDiffusion(double deltaTime);

    /**
     * @brief Calculate material-based reflection coefficient.
     * @param materialType Type of material hitting the wall.
     * @param impactEnergy Kinetic energy of the impact.
     * @return Reflection coefficient [0,1] based on material elasticity and impact energy.
     *
     * Calculates how much energy is reflected when material hits a wall.
     * Takes into account material elasticity and applies energy-dependent damping.
     */
    double calculateReflectionCoefficient(MaterialType materialType, double impactEnergy) const;

    // Queue of blocked transfers.
    std::vector<BlockedTransfer> blocked_transfers_;

private:
    WorldB& world_ref_; // Non-const reference for modifying cells.

    // Constants for pressure-driven flow.
    static constexpr double PRESSURE_FLOW_RATE = 1.0;    // Flow rate multiplier.
    static constexpr double PRESSURE_FORCE_SCALE = 1.0;   // Force scale factor.
    static constexpr double BACKGROUND_DECAY_RATE = 0.02; // 2% decay per timestep.
};
