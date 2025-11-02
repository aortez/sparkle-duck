#pragma once

#include "Vector2d.h"

/**
 * CellInterface provides a common interface for material addition operations
 * that works with both Cell (mixed materials) and Cell (pure materials).
 *
 * This allows WorldSetup to work polymorphically with different cell types
 * without needing to know the specific implementation details.
 */
class CellInterface {
public:
    virtual ~CellInterface() = default;

    // =================================================================
    // BASIC MATERIAL ADDITION
    // =================================================================

    // Add dirt material to the cell.
    virtual void addDirt(double amount) = 0;

    // Add water material to the cell.
    virtual void addWater(double amount) = 0;

    // =================================================================
    // ADVANCED MATERIAL ADDITION (with physics)
    // =================================================================

    // Add dirt with specific velocity (for particle effects).
    virtual void addDirtWithVelocity(double amount, const Vector2d& velocity) = 0;

    // Add water with specific velocity (for particle effects).
    virtual void addWaterWithVelocity(double amount, const Vector2d& velocity) = 0;

    // Add dirt with center of mass offset (for World's COM-based physics).
    virtual void addDirtWithCOM(double amount, const Vector2d& com, const Vector2d& velocity) = 0;

    // =================================================================
    // CELL STATE MANAGEMENT
    // =================================================================

    virtual void clear() = 0;

    // =================================================================
    // MATERIAL PROPERTIES
    // =================================================================

    // Get total material amount in cell (0.0 to 1.0+ depending on system).
    virtual double getTotalMaterial() const = 0;

    // Check if cell is effectively empty.
    virtual bool isEmpty() const = 0;

    // =================================================================
    // ASCII VISUALIZATION
    // =================================================================

    // Generate ASCII character representation of cell contents.
    virtual std::string toAsciiCharacter() const = 0;
};