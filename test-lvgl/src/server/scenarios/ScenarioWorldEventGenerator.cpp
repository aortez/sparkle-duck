#include "ScenarioWorldEventGenerator.h"
#include "core/Cell.h"
#include "core/PhysicsSettings.h"
#include "core/World.h"
#include "core/WorldData.h"

// Implementation file for ScenarioWorldEventGenerator

void ScenarioWorldEventGenerator::clear(World& world)
{
    // Reset all cells to empty state.
    for (uint32_t y = 0; y < world.getData().height; ++y) {
        for (uint32_t x = 0; x < world.getData().width; ++x) {
            world.getData().at(x, y) = DirtSim::Cell(); // Default empty cell.
        }
    }
}