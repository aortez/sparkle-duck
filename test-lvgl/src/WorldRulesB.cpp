#include "WorldRulesB.h"
#include "World.h"
#include "spdlog/spdlog.h"

#include <algorithm>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// RulesB Implementation (stubbed for now)
RulesB::RulesB()
{
    spdlog::info("Initialized World Rules B physics rules");
}

void RulesB::applyPhysics(
    Cell& cell, uint32_t x, uint32_t y, double deltaTimeSeconds, const World& world)
{
    // RulesB: Simplified physics similar to our new RulesBNew design
    
    if (cell.percentFull() < World::MIN_DIRT_THRESHOLD) return;

    // Apply gravity based on material density (simplified approach using total mass)
    double totalMass = cell.dirt + cell.water;
    cell.v.y += gravity_ * totalMass * deltaTimeSeconds;

    // Apply velocity limits from GridMechanics.md design
    // Max velocity is 0.9 cells per timestep  
    const double MAX_VELOCITY = 0.9;
    if (cell.v.mag() > MAX_VELOCITY) {
        cell.v = cell.v.normalize() * MAX_VELOCITY;
    }

    // If velocity > 0.5, slow down by 10%
    if (cell.v.mag() > 0.5) {
        cell.v *= 0.9;
    }

    // Update center of mass based on velocity
    cell.com += cell.v * deltaTimeSeconds;
    
    // Clamp COM to valid range [-1, 1] as per GridMechanics.md
    cell.com.x = std::clamp(cell.com.x, -1.0, 1.0);
    cell.com.y = std::clamp(cell.com.y, -1.0, 1.0);
}

void RulesB::updatePressures(World& world, double deltaTimeSeconds)
{
    // RulesB: Pressure system disabled for now - just clear all pressures
    for (uint32_t y = 0; y < world.getHeight(); ++y) {
        for (uint32_t x = 0; x < world.getWidth(); ++x) {
            world.at(x, y).pressure = Vector2d(0.0, 0.0);
        }
    }
}

void RulesB::applyPressureForces(World& world, double deltaTimeSeconds)
{
    // RulesB: Pressure forces disabled for now - no operation
}

bool RulesB::shouldTransfer(const Cell& cell, uint32_t x, uint32_t y, const World& world) const
{
    // RulesB: Transfer system disabled for now
    return false;
}

void RulesB::calculateTransferDirection(
    const Cell& cell,
    bool& shouldTransferX,
    bool& shouldTransferY,
    int& targetX,
    int& targetY,
    Vector2d& comOffset,
    uint32_t x,
    uint32_t y,
    const World& world) const
{
    // RulesB: Transfer system disabled for now
    shouldTransferX = false;
    shouldTransferY = false;
    targetX = x;
    targetY = y;
    comOffset = Vector2d(0.0, 0.0);
}

bool RulesB::attemptTransfer(
    Cell& cell,
    uint32_t x,
    uint32_t y,
    int targetX,
    int targetY,
    const Vector2d& comOffset,
    double totalMass,
    World& world)
{
    // RulesB: Transfer system disabled for now
    return false;
}

void RulesB::handleTransferFailure(
    Cell& cell,
    uint32_t x,
    uint32_t y,
    int targetX,
    int targetY,
    bool shouldTransferX,
    bool shouldTransferY,
    World& world)
{
    // RulesB: Transfer system disabled for now
}

void RulesB::handleBoundaryReflection(
    Cell& cell, int targetX, int targetY, bool shouldTransferX, bool shouldTransferY, World& world)
{
    // RulesB: Transfer system disabled for now
}

void RulesB::checkExcessiveDeflectionReflection(Cell& cell, World& world)
{
    // RulesB: Transfer system disabled for now
}

void RulesB::handleCollision(
    Cell& cell,
    uint32_t x,
    uint32_t y,
    int targetX,
    int targetY,
    bool shouldTransferX,
    bool shouldTransferY,
    const World& world)
{
    // RulesB: Transfer system disabled for now
}