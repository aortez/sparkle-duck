#include "WorldCalculatorBase.h"
#include "Cell.h"
#include "World.h"

using namespace DirtSim;

const Cell& WorldCalculatorBase::getCellAt(const World& world, uint32_t x, uint32_t y)
{
    return world.at(x, y);
}

bool WorldCalculatorBase::isValidCell(const World& world, int x, int y)
{
    return x >= 0 && y >= 0 && static_cast<uint32_t>(x) < world.getWidth()
        && static_cast<uint32_t>(y) < world.getHeight();
}