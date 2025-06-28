#pragma once

#include "MaterialMove.h"
#include "MaterialType.h"
#include "Vector2d.h"
#include "WorldBCalculatorBase.h"
#include "WorldInterface.h" // For PressureSystem enum
#include <cstdint>
#include <vector>

class CellB;
class WorldB;

/**
 * @brief Calculates pressure forces for WorldB physics
 *
 * This class encapsulates all pressure-related calculations including:
 * - Hydrostatic pressure (gravity-based weight distribution)
 * - Dynamic pressure (accumulated from blocked transfers)
 * - Combined pressure force application
 *
 * The pressure system implements dual physics following under_pressure.md:
 * 1. Hydrostatic: Slice-based calculation perpendicular to gravity
 * 2. Dynamic: Energy accumulation from blocked material transfers
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
     * @brief Main pressure application method.
     * @param deltaTime Time step for the current frame.
     */
    void applyPressure(double deltaTime);

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
     * @brief Apply combined pressure forces to all cells.
     * @param deltaTime Time step for force application.
     *
     * Combines hydrostatic and dynamic pressure forces based on material properties.
     * Updates cell velocities according to pressure gradients.
     */
    void applyDynamicPressureForces(double deltaTime);


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
     * @brief Calculate pressure-driven material flows.
     * @param deltaTime Time step for the current frame.
     * @return Vector of MaterialMove objects representing pressure-driven flows.
     *
     * Analyzes pressure gradients across the grid and generates material transfers
     * from high pressure to low pressure regions. Material flows down the pressure
     * gradient (from high to low).
     */
    std::vector<MaterialMove> calculatePressureFlow(double deltaTime);

    /**
     * @brief Apply pressure forces to cell velocities and handle pressure decay.
     * @param deltaTime Time step for the current frame.
     *
     * Converts dynamic pressure into velocity changes and applies pressure decay.
     * This method handles:
     * - Converting pressure to forces based on pressure gradients
     * - Applying forces to cell velocities
     * - Decaying dynamic pressure over time
     * - Managing debug visualization state
     */
    void applyPressureForces(double deltaTime);

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

    // Queue of blocked transfers.
    std::vector<BlockedTransfer> blocked_transfers_;

private:
    WorldB& world_ref_; // Non-const reference for modifying cells.

    // Constants for pressure-driven flow.
    static constexpr double PRESSURE_FLOW_RATE = 1.0;    // Flow rate multiplier.
    static constexpr double PRESSURE_FORCE_SCALE = 1.0;   // Force scale factor.
    static constexpr double BACKGROUND_DECAY_RATE = 0.02; // 2% decay per timestep.
};
