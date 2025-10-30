#pragma once

#include <cstdint>

// Forward declarations.
class Cell;
class World;

/**
 * @brief Base class for World calculator classes.
 *
 * This abstract base class provides common functionality for all World
 * calculator classes including:
 * - Grid access and boundary checking
 * - Common constants and thresholds
 * - Consistent constructor and ownership patterns
 *
 * Derived calculator classes should inherit from this base and implement
 * their specific calculation logic while reusing the common infrastructure.
 */
class WorldCalculatorBase {
public:
    /**
     * @brief Constructor takes a World for accessing world data.
     * @param world World providing access to grid and cells
     */
    explicit WorldCalculatorBase(const World& world);

    // Disable copy construction and assignment.
    WorldCalculatorBase(const WorldCalculatorBase&) = delete;
    WorldCalculatorBase& operator=(const WorldCalculatorBase&) = delete;

    // Allow move construction and assignment.
    WorldCalculatorBase(WorldCalculatorBase&&) = default;
    WorldCalculatorBase& operator=(WorldCalculatorBase&&) = default;

    // Virtual destructor for proper cleanup.
    virtual ~WorldCalculatorBase() = default;

    // Common constants used across calculator classes.
    static constexpr double MIN_MATTER_THRESHOLD = 0.001; // Minimum matter to process.

protected:
    // Reference to the world for accessing grid data.
    const World& world_;

    /**
     * @brief Get cell at specific coordinates.
     * @param x Column coordinate
     * @param y Row coordinate
     * @return Reference to cell at (x,y)
     */
    const Cell& getCellAt(uint32_t x, uint32_t y) const;

    /**
     * @brief Check if coordinates are valid for the grid.
     * @param x Column coordinate
     * @param y Row coordinate
     * @return true if coordinates are within bounds
     */
    bool isValidCell(int x, int y) const;
};