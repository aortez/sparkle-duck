#include "MaterialNeighborhood.h"

namespace DirtSim {

MaterialType MaterialNeighborhood::getMaterial(int dx, int dy) const
{
    int bit_pos = (dy + 1) * 3 + (dx + 1);
    return getMaterialByBitPos(bit_pos);
}

MaterialType MaterialNeighborhood::getMaterialByBitPos(int bit_pos) const
{
    int shift = bit_pos * BITS_PER_MATERIAL;
    return static_cast<MaterialType>((data_ >> shift) & 0xF);
}

MaterialType MaterialNeighborhood::getCenterMaterial() const
{
    return getMaterialByBitPos(4);
}

MaterialType MaterialNeighborhood::north() const
{
    return getMaterial(0, -1);
}

MaterialType MaterialNeighborhood::south() const
{
    return getMaterial(0, 1);
}

MaterialType MaterialNeighborhood::east() const
{
    return getMaterial(1, 0);
}

MaterialType MaterialNeighborhood::west() const
{
    return getMaterial(-1, 0);
}

MaterialType MaterialNeighborhood::northEast() const
{
    return getMaterial(1, -1);
}

MaterialType MaterialNeighborhood::northWest() const
{
    return getMaterial(-1, -1);
}

MaterialType MaterialNeighborhood::southEast() const
{
    return getMaterial(1, 1);
}

MaterialType MaterialNeighborhood::southWest() const
{
    return getMaterial(-1, 1);
}

int MaterialNeighborhood::countMaterial(MaterialType material) const
{
    int count = 0;
    for (int i = 0; i < 9; ++i) {
        if (i == 4) continue; // Skip center.
        MaterialType mat = static_cast<MaterialType>((data_ >> (i * BITS_PER_MATERIAL)) & 0xF);
        if (mat == material) {
            ++count;
        }
    }
    return count;
}

bool MaterialNeighborhood::allNeighborsSameMaterial(MaterialType material) const
{
    for (int i = 0; i < 9; ++i) {
        if (i == 4) continue; // Skip center.
        MaterialType mat = static_cast<MaterialType>((data_ >> (i * BITS_PER_MATERIAL)) & 0xF);
        if (mat != material) {
            return false;
        }
    }
    return true;
}

bool MaterialNeighborhood::isSurroundedBySameMaterial() const
{
    MaterialType center_mat = getCenterMaterial();
    return allNeighborsSameMaterial(center_mat);
}

} // namespace DirtSim
