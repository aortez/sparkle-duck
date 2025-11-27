#include "EmptyNeighborhood.h"

namespace DirtSim {

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
