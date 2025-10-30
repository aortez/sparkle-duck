#include "WorldCalculatorBase.h"
#include "Cell.h"
#include "World.h"

WorldCalculatorBase::WorldCalculatorBase(const World& world) : world_(world)
{}

const Cell& WorldCalculatorBase::getCellAt(uint32_t x, uint32_t y) const
{
    // Direct access to Cell through World.
    return world_.at(x, y);
}

bool WorldCalculatorBase::isValidCell(int x, int y) const
{
    return x >= 0 && y >= 0 && static_cast<uint32_t>(x) < world_.getWidth()
        && static_cast<uint32_t>(y) < world_.getHeight();
}