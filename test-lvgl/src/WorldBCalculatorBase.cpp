#include "WorldBCalculatorBase.h"
#include "CellB.h"
#include "WorldB.h"

WorldBCalculatorBase::WorldBCalculatorBase(const WorldB& world) : world_(world)
{}

const CellB& WorldBCalculatorBase::getCellAt(uint32_t x, uint32_t y) const
{
    // Direct access to CellB through WorldB
    return world_.at(x, y);
}

bool WorldBCalculatorBase::isValidCell(int x, int y) const
{
    return x >= 0 && y >= 0 && static_cast<uint32_t>(x) < world_.getWidth()
        && static_cast<uint32_t>(y) < world_.getHeight();
}