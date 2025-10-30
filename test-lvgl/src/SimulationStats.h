#pragma once

#include <chrono>
#include <cstdint>

/**
 * @brief Statistics about the current simulation state.
 *
 * This struct holds aggregate data about the simulation that may be
 * expensive to compute, so it's only updated periodically rather than
 * every frame. All members are designed to be safely copyable for
 * thread-safe access through SharedSimState.
 */
struct SimulationStats {
    // Cell counts by state
    uint32_t totalCells = 0;  ///< Total cells in grid (width * height).
    uint32_t activeCells = 0; ///< Cells with material (non-empty).
    uint32_t emptyCells = 0;  ///< Cells without material.

    // Material counts (for World)
    uint32_t airCells = 0;   ///< Cells with AIR material.
    uint32_t dirtCells = 0;  ///< Cells with DIRT material.
    uint32_t waterCells = 0; ///< Cells with WATER material.
    uint32_t woodCells = 0;  ///< Cells with WOOD material.
    uint32_t sandCells = 0;  ///< Cells with SAND material.
    uint32_t metalCells = 0; ///< Cells with METAL material.
    uint32_t leafCells = 0;  ///< Cells with LEAF material.
    uint32_t wallCells = 0;  ///< Cells with WALL material.

    // Mass and physics
    double totalMass = 0.0;          ///< Total mass of all materials.
    double totalKineticEnergy = 0.0; ///< Total kinetic energy in system.
    double avgVelocity = 0.0;        ///< Average velocity magnitude.
    double maxVelocity = 0.0;        ///< Maximum velocity magnitude.

    // Pressure statistics
    double avgPressure = 0.0; ///< Average pressure across all cells.
    double maxPressure = 0.0; ///< Maximum pressure in any cell.
    double minPressure = 0.0; ///< Minimum pressure in any cell.

    // Simulation progress
    uint32_t stepCount = 0;      ///< Current simulation timestep.
    double simulationTime = 0.0; ///< Total simulated time in seconds.

    // Performance metrics
    double avgStepTime = 0.0;    ///< Average time per simulation step (ms).
    double lastStepTime = 0.0;   ///< Time for last simulation step (ms).
    uint32_t stepsPerSecond = 0; ///< Simulation steps completed per second.

    // Update tracking
    std::chrono::steady_clock::time_point lastUpdate; ///< When stats were last computed.
    uint32_t updateCount = 0;                         ///< Number of times stats have been updated.

    /**
     * @brief Check if stats need updating based on time since last update.
     * @param updateInterval Minimum time between updates (default 100ms).
     * @return true if stats should be recomputed.
     */
    bool needsUpdate(
        std::chrono::milliseconds updateInterval = std::chrono::milliseconds(100)) const
    {
        auto now = std::chrono::steady_clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(now - lastUpdate)
            >= updateInterval;
    }

    /**
     * @brief Mark stats as updated with current timestamp.
     */
    void markUpdated()
    {
        lastUpdate = std::chrono::steady_clock::now();
        updateCount++;
    }
};