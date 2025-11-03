#pragma once

#include <cstdint>

// Forward declarations.
namespace DirtSim {
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
    // Default constructor - calculators are now stateless.
    WorldCalculatorBase() = default;

    // Virtual destructor for proper cleanup.
    virtual ~WorldCalculatorBase() = default;

    // Common constants used across calculator classes.
    static constexpr double MIN_MATTER_THRESHOLD = 0.001; // Minimum matter to process.

protected:
    /**
     * @brief Get cell at specific coordinates.
     * @param world World providing access to grid and cells
     * @param x Column coordinate
     * @param y Row coordinate
     * @return Reference to cell at (x,y)
     */
    static const Cell& getCellAt(const World& world, uint32_t x, uint32_t y);

    /**
     * @brief Check if coordinates are valid for the grid.
     * @param world World providing access to grid dimensions
     * @param x Column coordinate
     * @param y Row coordinate
     * @return true if coordinates are within bounds
     */
    static bool isValidCell(const World& world, int x, int y);
};

} // namespace DirtSim