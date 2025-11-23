#pragma once

#include "core/MaterialType.h"

#include <cstdint>

namespace DirtSim {

/**
 * Typed wrapper for 3×3 neighborhood of material types.
 *
 * Packs 9 material types (4 bits each) into 36 bits of a uint64_t:
 *   Bits 0-3:   NW material type
 *   Bits 4-7:   N  material type
 *   Bits 8-11:  NE material type
 *   Bits 12-15: W  material type
 *   Bits 16-19: C  material type (center)
 *   Bits 20-23: E  material type
 *   Bits 24-27: SW material type
 *   Bits 28-31: S  material type
 *   Bits 32-35: SE material type
 *   Bits 36-63: Unused (28 bits for future expansion)
 *
 * Bit layout matches Neighborhood3x3:
 *   NW N  NE     Bit groups:
 *   W  C  E      0  1  2
 *   SW S  SE     3  4  5
 *                6  7  8
 *
 * This enables zero-lookup material queries - instead of world.at(x,y).material_type,
 * get material directly from the precomputed neighborhood.
 */
class MaterialNeighborhood {
private:
    uint64_t data_;

    static constexpr int BITS_PER_MATERIAL = 4;

public:
    // Constructor from packed uint64_t.
    explicit MaterialNeighborhood(uint64_t data) : data_(data) {}

    // Expose raw data for advanced use cases.
    uint64_t raw() const { return data_; }

    // ========== Material Access Methods ==========

    /**
     * Get material type at offset from center.
     *
     * @param dx X offset from center [-1, 1]
     * @param dy Y offset from center [-1, 1]
     * @return MaterialType at that position
     */
    inline MaterialType getMaterial(int dx, int dy) const
    {
        int bit_pos = (dy + 1) * 3 + (dx + 1);
        return getMaterialByBitPos(bit_pos);
    }

    inline MaterialType getMaterialByBitPos(int bit_pos) const
    {
        int shift = bit_pos * BITS_PER_MATERIAL;
        return static_cast<MaterialType>((data_ >> shift) & 0xF);
    }

    inline MaterialType getCenterMaterial() const { return getMaterialByBitPos(4); }

    // ========== Named Directional Accessors ==========

    MaterialType north() const;
    MaterialType south() const;
    MaterialType east() const;
    MaterialType west() const;
    MaterialType northEast() const;
    MaterialType northWest() const;
    MaterialType southEast() const;
    MaterialType southWest() const;

    // ========== Aggregate Query Methods ==========

    /**
     * Count how many neighbors are a specific material type.
     *
     * @param material Material type to count
     * @return Number of neighbors (0-8) matching the material
     */
    int countMaterial(MaterialType material) const;

    /**
     * Check if all neighbors are the same material type.
     *
     * Useful for cohesion calculations.
     *
     * @param material Material type to check
     * @return true if all 8 neighbors match the material
     */
    bool allNeighborsSameMaterial(MaterialType material) const;

    /**
     * Check if center is surrounded by same material.
     *
     * @return true if all 8 neighbors have same material as center
     */
    bool isSurroundedBySameMaterial() const;
};

} // namespace DirtSim
