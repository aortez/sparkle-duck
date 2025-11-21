#include "EmptyNeighborhood.h"

namespace DirtSim {

bool EmptyNeighborhood::isEmpty(int dx, int dy) const
{
    return data_.isValidAt(dx, dy) && data_.getAt(dx, dy);
}

bool EmptyNeighborhood::hasMaterial(int dx, int dy) const
{
    return data_.isValidAt(dx, dy) && !data_.getAt(dx, dy);
}

uint16_t EmptyNeighborhood::getValidWithMaterialMask() const
{
    uint16_t valid = data_.getValidLayer();
    uint16_t empty = data_.getValueLayer();
    return valid & ~empty;
}

bool EmptyNeighborhood::centerHasMaterial() const
{
    return (getValidWithMaterialMask() & (1 << 4)) != 0;
}

uint16_t EmptyNeighborhood::getMaterialNeighborsBitGrid() const
{
    return getValidWithMaterialMask() & ~(1 << 4);
}

int EmptyNeighborhood::countMaterialNeighbors() const
{
    return __builtin_popcount(getMaterialNeighborsBitGrid());
}

} // namespace DirtSim
