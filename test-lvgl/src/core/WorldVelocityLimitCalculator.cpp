#include "WorldVelocityLimitCalculator.h"
#include "Cell.h"
#include "MaterialType.h"
#include "World.h"
#include "WorldData.h"
#include <spdlog/spdlog.h>

namespace DirtSim {

// Maximum velocity to prevent skipping cells.
static constexpr double MAX_VELOCITY_PER_TIMESTEP = 200.0;

// Velocity above which damping kicks in.
static constexpr double DAMPING_THRESHOLD_PER_TIMESTEP = 100.0;

// Damping factor applied each timestep when above threshold.
static constexpr double DAMPING_FACTOR_PER_TIMESTEP = 0.05;

void WorldVelocityLimitCalculator::limitVelocity(Cell& cell, double /* deltaTime */) const
{
    const double speed = cell.velocity.mag();

    if (speed > MAX_VELOCITY_PER_TIMESTEP) {
        cell.velocity = cell.velocity * (MAX_VELOCITY_PER_TIMESTEP / speed);
    }

    if (speed > DAMPING_THRESHOLD_PER_TIMESTEP) {
        Vector2d old_velocity = cell.velocity;
        cell.velocity = cell.velocity * (1.0 - DAMPING_FACTOR_PER_TIMESTEP);
        spdlog::debug(
            "{} velocity damped: {:.3f} -> {:.3f} (above threshold {:.1f})",
            getMaterialName(cell.material_type),
            old_velocity.magnitude(),
            cell.velocity.magnitude(),
            DAMPING_THRESHOLD_PER_TIMESTEP);
    }
}

void WorldVelocityLimitCalculator::processAllCells(World& world, double deltaTime) const
{
    WorldData& data = world.getData();
    for (auto& cell : data.cells) {
        if (!cell.isEmpty()) {
            limitVelocity(cell, deltaTime);
        }
    }
}

} // namespace DirtSim
