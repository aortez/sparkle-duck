#include "Scenario.h"
#include "core/World.h"
#include "core/WorldEventGenerator.h"

// Default implementations delegate to old WorldEventGenerator during migration.

void Scenario::setup(World& world)
{
    if (!generator_) {
        generator_ = createWorldEventGenerator();
    }
    generator_->setup(world);
}

void Scenario::reset(World& world)
{
    generator_ = createWorldEventGenerator();
    generator_->setup(world);
}

void Scenario::tick(World& world, double deltaTime)
{
    if (generator_) {
        generator_->addParticles(world, world.data.timestep, deltaTime);
    }
}
