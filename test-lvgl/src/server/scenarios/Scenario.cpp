#include "Scenario.h"
#include "core/World.h"

void Scenario::setup(World& world)
{
    // Default implementation: delegate to old generator during migration.
    if (!generator_) {
        generator_ = createWorldEventGenerator();
    }
    if (generator_) {
        generator_->setup(world);
    }
}

void Scenario::reset(World& world)
{
    // Default implementation: recreate generator and call setup.
    generator_ = createWorldEventGenerator();
    setup(world);
}

void Scenario::tick(World& world, double deltaTime)
{
    // Default implementation: delegate to old generator during migration.
    if (!generator_) {
        generator_ = createWorldEventGenerator();
    }
    if (generator_) {
        // WorldEventGenerator uses addParticles instead of tick.
        // Pass timestep as 0 since we don't track it at this level.
        generator_->addParticles(world, 0, deltaTime);
    }
}
