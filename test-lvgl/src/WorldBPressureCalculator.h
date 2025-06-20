#pragma once

#include "WorldBCalculatorBase.h"
#include "MaterialType.h"
#include "Vector2d.h"
#include "WorldInterface.h"  // For PressureSystem enum
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
     * @brief Constructor takes a WorldB for accessing and modifying world data
     * @param world WorldB providing access to grid and cells (non-const for modifications)
     */
    explicit WorldBPressureCalculator(WorldB& world);

    // Blocked transfer data for dynamic pressure accumulation
    struct BlockedTransfer {
        int fromX, fromY;          // Source cell coordinates
        int toX, toY;              // Target cell coordinates  
        double transfer_amount;    // Amount that was blocked
        Vector2d velocity;         // Velocity at time of blocking
        double energy;             // Kinetic energy of blocked transfer
    };

    // Pressure-specific constants
    static constexpr double SLICE_THICKNESS = 1.0;           // Thickness of pressure slices
    static constexpr double HYDROSTATIC_MULTIPLIER = 0.002;  // Hydrostatic force strength
    static constexpr double DYNAMIC_MULTIPLIER = 0.1;        // Dynamic force strength
    static constexpr double DYNAMIC_ACCUMULATION_RATE = 0.05; // Rate of pressure buildup
    static constexpr double DYNAMIC_DECAY_RATE = 0.02;       // Rate of pressure dissipation
    static constexpr double MIN_PRESSURE_THRESHOLD = 0.01;   // Ignore pressures below this

    /**
     * @brief Main pressure application method
     * @param deltaTime Time step for the current frame
     */
    void applyPressure(double deltaTime);

    /**
     * @brief Calculate hydrostatic pressure for all cells
     * 
     * Implements slice-based calculation perpendicular to gravity direction.
     * Pressure accumulates based on material density and gravity magnitude.
     */
    void calculateHydrostaticPressure();

    /**
     * @brief Queue a blocked transfer for dynamic pressure accumulation
     * @param transfer BlockedTransfer data including source, target, and energy
     */
    void queueBlockedTransfer(const BlockedTransfer& transfer);

    /**
     * @brief Process blocked transfers and accumulate dynamic pressure
     * 
     * Converts blocked kinetic energy into dynamic pressure at source cells.
     * Updates pressure gradients based on blocked transfer directions.
     */
    void processBlockedTransfers();

    /**
     * @brief Apply combined pressure forces to all cells
     * @param deltaTime Time step for force application
     * 
     * Combines hydrostatic and dynamic pressure forces based on material properties.
     * Updates cell velocities according to pressure gradients.
     */
    void applyDynamicPressureForces(double deltaTime);

    /**
     * @brief Calculate combined pressure force for a cell
     * @param cell Cell to calculate force for
     * @return Combined pressure force vector
     */
    Vector2d calculatePressureForce(const CellB& cell) const;

    /**
     * @brief Get material-specific hydrostatic pressure sensitivity
     * @param type Material type
     * @return Weight factor for hydrostatic pressure [0,1]
     */
    double getHydrostaticWeight(MaterialType type) const;

    /**
     * @brief Get material-specific dynamic pressure sensitivity
     * @param type Material type
     * @return Weight factor for dynamic pressure [0,1]
     */
    double getDynamicWeight(MaterialType type) const;

    /**
     * @brief Clear all blocked transfers (for testing)
     */
    void clearBlockedTransfers() { blocked_transfers_.clear(); }

    /**
     * @brief Get count of pending blocked transfers (for testing)
     */
    size_t getBlockedTransferCount() const { return blocked_transfers_.size(); }

private:
    WorldB& world_ref_;  // Non-const reference for modifying cells
    std::vector<BlockedTransfer> blocked_transfers_; // Queue of blocked transfers
};