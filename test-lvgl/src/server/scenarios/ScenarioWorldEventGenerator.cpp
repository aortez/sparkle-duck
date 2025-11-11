#include "ScenarioWorldEventGenerator.h"
#include "core/Cell.h"
#include "core/World.h"

// Implementation file for ScenarioWorldEventGenerator

void ScenarioWorldEventGenerator::clear(World& world)
{
    // Reset all cells to empty state.
    for (uint32_t y = 0; y < world.data.height; ++y) {
        for (uint32_t x = 0; x < world.data.width; ++x) {
            world.at(x, y) = DirtSim::Cell(); // Default empty cell.
        }
    }
}