#pragma once

#include "WorldCalculatorBase.h"
#include <cstdint>

namespace DirtSim {

class Cell;
class World;

class WorldVelocityLimitCalculator : public WorldCalculatorBase {
public:
    WorldVelocityLimitCalculator() = default;

    void limitVelocity(Cell& cell, double deltaTime) const;

    void processAllCells(World& world, double deltaTime) const;
};

} // namespace DirtSim
