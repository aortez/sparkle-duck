#pragma once

#include "MaterialMove.h"
#include "MaterialType.h"
#include "Vector2d.h"
#include "WorldCalculatorBase.h"

#include <cstdint>
#include <vector>

namespace DirtSim {

class Cell;
class World;

/**
 * @brief Calculates pressure forces for World physics.
 *
 * See GridMechanics.md for more info.
 *
 */
class WorldPressureCalculator : public WorldCalculatorBase {
public:
    /**
     * @brief Enum for pressure gradient calculation directions.
     */
    enum class PressureGradientDirections { Four, Eight };

    // Default constructor - calculator is stateless.
    WorldPressureCalculator() = default;

    // Blocked transfer data for dynamic pressure accumulation.
    struct BlockedTransfer {
        int fromX, fromY;       // Source cell coordinates.
        int toX, toY;           // Target cell coordinates.
        double transfer_amount; // Amount that was blocked.
        Vector2d velocity;      // Velocity at time of blocking.
        double energy;          // Kinetic energy of blocked transfer.
    };

    // Pressure-specific constants.
    static constexpr double SLICE_THICKNESS = 1.0;
    static constexpr double HYDROSTATIC_MULTIPLIER = 1.0;
    static constexpr double DYNAMIC_MULTIPLIER = 1;
    static constexpr double DYNAMIC_DECAY_RATE = 0.1;
    static constexpr double MIN_PRESSURE_THRESHOLD = 0.001; // Ignore pressures below this.

    /**
     * @brief Calculate hydrostatic pressure for all cells.
     *
     * Implements slice-based calculation perpendicular to gravity direction.
     * Pressure accumulates based on material density and gravity magnitude.
     *
     * @param world World providing access to grid and cells (non-const for modifications).
     */
    void calculateHydrostaticPressure(World& world);

    /**
     * @brief Queue a blocked transfer for dynamic pressure accumulation.
     * @param transfer BlockedTransfer data including source, target, and energy.
     */
    void queueBlockedTransfer(const BlockedTransfer& transfer);

    /**
     * @brief Process blocked transfers and accumulate dynamic pressure.
     * @param world World providing access to grid and cells (non-const for modifications).
     * @param blocked_transfers Vector of blocked transfers to process.
     *
     * Converts blocked kinetic energy into dynamic pressure at source cells.
     * Updates pressure gradients based on blocked transfer directions.
     */
    void processBlockedTransfers(
        World& world, const std::vector<BlockedTransfer>& blocked_transfers);

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
     * @param world World providing access to grid and cells.
     * @param x X coordinate of cell.
     * @param y Y coordinate of cell.
     * @return Pressure gradient vector pointing from high to low pressure.
     *
     * Calculates the pressure gradient by comparing total pressure (hydrostatic + dynamic)
     * with neighboring cells. The gradient points in the direction of decreasing pressure.
     */
    Vector2d calculatePressureGradient(const World& world, uint32_t x, uint32_t y) const;

    /**
     * @brief Calculate expected gravity gradient at a cell position.
     * @param world World providing access to grid and cells.
     * @param x X coordinate of cell.
     * @param y Y coordinate of cell.
     * @return Gravity gradient vector representing expected equilibrium pressure gradient.
     *
     * Calculates the expected pressure gradient due to gravity based on material density
     * differences with neighbors. In equilibrium, this should balance the pressure gradient.
     */
    Vector2d calculateGravityGradient(const World& world, uint32_t x, uint32_t y) const;

    /**
     * @brief Generate virtual gravity transfers for pressure accumulation.
     * @param world World providing access to grid and cells (non-const for modifications).
     * @param deltaTime Time step for the current frame.
     *
     * Creates virtual blocked transfers from gravity forces acting on material.
     * Even when material is at rest, gravity is always trying to pull it down.
     * If the downward path is blocked, this gravitational force converts to pressure.
     * This allows dynamic pressure to naturally model hydrostatic-like behavior.
     */
    void generateVirtualGravityTransfers(World& world, double deltaTime);

    /**
     * @brief Apply pressure decay to dynamic pressure values.
     * @param world World providing access to grid and cells (non-const for modifications).
     * @param deltaTime Time step for the current frame.
     *
     * Decays dynamic pressure over time. Hydrostatic pressure does not decay.
     * This should be called after material moves are complete.
     */
    void applyPressureDecay(World& world, double deltaTime);

    /**
     * @brief Apply pressure diffusion between neighboring cells.
     * @param world World providing access to grid and cells (non-const for modifications).
     * @param deltaTime Time step for the current frame.
     *
     * Implements material-specific pressure propagation using 4-neighbor diffusion.
     * Pressure spreads from high to low pressure regions based on material
     * diffusion coefficients. Walls act as barriers with zero flux.
     */
    void applyPressureDiffusion(World& world, double deltaTime);

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
    // Configuration for pressure gradient calculation.
    PressureGradientDirections gradient_directions_ = PressureGradientDirections::Eight;

    // Constants for pressure-driven flow.
    static constexpr double PRESSURE_FLOW_RATE = 1.0;     // Flow rate multiplier.
    static constexpr double BACKGROUND_DECAY_RATE = 0.02; // 2% decay per timestep.

    /**
     * @brief Check if a material type provides rigid structural support.
     * @param type Material type to check.
     * @return True if material can support weight above it.
     */
    bool isRigidSupport(MaterialType type) const;

    /**
     * @brief Get surrounding fluid density for buoyancy calculation.
     * @param world World providing access to grid and cells.
     * @param x X coordinate of cell.
     * @param y Y coordinate of cell.
     * @return Average density of surrounding fluid materials.
     *
     * Checks all 8 neighbors and returns average density of fluid materials (WATER, AIR).
     * Used when USE_COLUMN_BASED_BUOYANCY = false for more accurate buoyancy at
     * horizontal fluid boundaries. Returns 1.0 (water density) if no fluid neighbors found.
     */
    double getSurroundingFluidDensity(const World& world, uint32_t x, uint32_t y) const;
};

} // namespace DirtSim
