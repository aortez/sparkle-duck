#pragma once

#include <cstdint>

// Forward declarations
class CellB;
class WorldB;

/**
 * @brief Base class for WorldB calculator classes
 *
 * This abstract base class provides common functionality for all WorldB
 * calculator classes including:
 * - Grid access and boundary checking
 * - Common constants and thresholds
 * - Consistent constructor and ownership patterns
 *
 * Derived calculator classes should inherit from this base and implement
 * their specific calculation logic while reusing the common infrastructure.
 */
class WorldBCalculatorBase {
public:
    /**
     * @brief Constructor takes a WorldB for accessing world data
     * @param world WorldB providing access to grid and cells
     */
    explicit WorldBCalculatorBase(const WorldB& world);

    // Disable copy construction and assignment
    WorldBCalculatorBase(const WorldBCalculatorBase&) = delete;
    WorldBCalculatorBase& operator=(const WorldBCalculatorBase&) = delete;

    // Allow move construction and assignment
    WorldBCalculatorBase(WorldBCalculatorBase&&) = default;
    WorldBCalculatorBase& operator=(WorldBCalculatorBase&&) = default;

    // Virtual destructor for proper cleanup
    virtual ~WorldBCalculatorBase() = default;

    // Common constants used across calculator classes
    static constexpr double MIN_MATTER_THRESHOLD = 0.001; // Minimum matter to process

protected:
    // Reference to the world for accessing grid data
    const WorldB& world_;

    /**
     * @brief Get cell at specific coordinates
     * @param x Column coordinate
     * @param y Row coordinate
     * @return Reference to cell at (x,y)
     */
    const CellB& getCellAt(uint32_t x, uint32_t y) const;

    /**
     * @brief Check if coordinates are valid for the grid
     * @param x Column coordinate
     * @param y Row coordinate
     * @return true if coordinates are within bounds
     */
    bool isValidCell(int x, int y) const;
};