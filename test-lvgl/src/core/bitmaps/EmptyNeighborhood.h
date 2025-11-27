#pragma once

#include "Neighborhood3x3.h"

namespace DirtSim {

/**
 * Typed wrapper around Neighborhood3x3 for "empty cell" semantics.
 *
 * This zero-cost abstraction provides domain-specific methods for
 * interpreting neighborhood data where the value bits represent
 * cell emptiness (1 = empty, 0 = has material).
 *
 * The wrapper compiles to identical code as direct Neighborhood3x3
 * access but provides clearer semantics and type safety.
 */
class EmptyNeighborhood {
private:
    Neighborhood3x3 data_;

public:
    // Constructor from raw Neighborhood3x3.
    explicit EmptyNeighborhood(Neighborhood3x3 n) : data_(n) {}

    // Expose raw data for advanced use cases.
    const Neighborhood3x3& raw() const { return data_; }

    // ========== Domain-Specific Query Methods ==========

    /**
     * Check if neighbor position exists (is in-bounds).
     *
     * @param dx X offset from center [-1, 1]
     * @param dy Y offset from center [-1, 1]
     * @return true if neighbor is within grid bounds
     */
    bool exists(int dx, int dy) const { return data_.isValidAt(dx, dy); }

    /**
     * Check if neighbor exists and is empty (no material).
     *
     * @param dx X offset from center [-1, 1]
     * @param dy Y offset from center [-1, 1]
     * @return true if neighbor is in-bounds and has fill_ratio == 0
     */
    inline bool isEmpty(int dx, int dy) const
    {
        return data_.isValidAt(dx, dy) && data_.getAt(dx, dy);
    }

    /**
     * Check if neighbor exists and has material (not empty).
     *
     * @param dx X offset from center [-1, 1]
     * @param dy Y offset from center [-1, 1]
     * @return true if neighbor is in-bounds and has fill_ratio > 0
     */
    inline bool hasMaterial(int dx, int dy) const
    {
        return data_.isValidAt(dx, dy) && !data_.getAt(dx, dy);
    }

    // ========== Optimized Mask Helpers ==========

    /**
     * Get bitmask of cells that are valid and have material (not empty).
     *
     * This precomputes valid & ~empty, which is useful for many operations.
     * Bits 0-8 represent the 3×3 grid, 1 = valid cell with material.
     *
     * @return Bitmask where 1 = cell exists and has material
     */
    uint16_t getValidWithMaterialMask() const;

    /**
     * Check if center cell is valid and has material.
     *
     * Equivalent to: centerValid() && !center(), but faster.
     * Uses precomputed mask for single-operation check.
     *
     * @return true if center exists and is not empty
     */
    bool centerHasMaterial() const;

    /**
     * Get bitmask of neighbors with material (excluding center).
     *
     * Returns a 9-bit grid where 1 = neighbor has material, 0 = empty or OOB.
     * Center (bit 4) is always 0. Ready for bit scanning iteration.
     *
     * @return Bitmask of material neighbors
     */
    uint16_t getMaterialNeighborsBitGrid() const;

    // ========== Aggregate Query Methods ==========

    /**
     * Count how many valid neighbors exist (excluding center).
     *
     * @return Number of in-bounds neighbors [0-8]
     */
    int countValidNeighbors() const { return data_.countValidNeighbors(); }

    /**
     * Count how many neighbors have material (excluding center).
     *
     * Uses fast popcount on precomputed bitmap.
     *
     * @return Number of non-empty neighbors [0-8]
     */
    int countMaterialNeighbors() const;

    /**
     * Count how many neighbors are empty (excluding center).
     *
     * @return Number of empty neighbors [0-8]
     */
    int countEmptyNeighbors() const { return data_.countTrueNeighbors(); }

    /**
     * Check if all valid neighbors are empty.
     *
     * @return true if every in-bounds neighbor has fill_ratio == 0
     */
    bool allNeighborsEmpty() const { return data_.allValidNeighborsTrue(); }

    // ========== Named Directional Accessors ==========

    bool northExists() const { return data_.northValid(); }
    bool southExists() const { return data_.southValid(); }
    bool eastExists() const { return data_.eastValid(); }
    bool westExists() const { return data_.westValid(); }

    bool northIsEmpty() const { return data_.northValid() && data_.north(); }
    bool southIsEmpty() const { return data_.southValid() && data_.south(); }
    bool eastIsEmpty() const { return data_.eastValid() && data_.east(); }
    bool westIsEmpty() const { return data_.westValid() && data_.west(); }

    bool northHasMaterial() const { return data_.northValid() && !data_.north(); }
    bool southHasMaterial() const { return data_.southValid() && !data_.south(); }
    bool eastHasMaterial() const { return data_.eastValid() && !data_.east(); }
    bool westHasMaterial() const { return data_.westValid() && !data_.west(); }
};

} // namespace DirtSim
