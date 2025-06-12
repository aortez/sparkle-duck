#include "RulesBNew.h"
#include "WorldB.h"
#include "spdlog/spdlog.h"

#include <algorithm>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

RulesBNew::RulesBNew()
{
    spdlog::info("Initialized RulesBNew physics rules with material types");
}

void RulesBNew::applyPhysics(
    CellB& cell, uint32_t x, uint32_t y, double deltaTimeSeconds, const WorldB& world)
{
    if (cell.isEmpty() || cell.isWall()) return;

    // Apply gravity based on material density
    double materialDensity = cell.getDensity();
    cell.v.y += gravity_ * materialDensity * deltaTimeSeconds;

    // Apply velocity limits from design document
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

void RulesBNew::updatePressures(WorldB& world, double deltaTimeSeconds)
{
    // Pressure system removed for now - just clear all pressures
    for (uint32_t y = 0; y < world.getHeight(); ++y) {
        for (uint32_t x = 0; x < world.getWidth(); ++x) {
            world.at(x, y).pressure = Vector2d(0.0, 0.0);
        }
    }
}

void RulesBNew::applyPressureForces(WorldB& world, double deltaTimeSeconds)
{
    // Pressure forces removed for now - no operation
}

bool RulesBNew::shouldTransfer(const CellB& cell, uint32_t x, uint32_t y, const WorldB& world) const
{
    // Transfer system disabled for now
    return false;
}

void RulesBNew::calculateTransferDirection(
    const CellB& cell,
    bool& shouldTransferX,
    bool& shouldTransferY,
    int& targetX,
    int& targetY,
    Vector2d& comOffset,
    uint32_t x,
    uint32_t y,
    const WorldB& world) const
{
    // Transfer system disabled for now
    shouldTransferX = false;
    shouldTransferY = false;
    targetX = x;
    targetY = y;
    comOffset = Vector2d(0.0, 0.0);
}

bool RulesBNew::attemptTransfer(
    CellB& cell,
    uint32_t x,
    uint32_t y,
    int targetX,
    int targetY,
    const Vector2d& comOffset,
    double totalMass,
    WorldB& world)
{
    // Transfer system disabled for now
    return false;
}

void RulesBNew::handleTransferFailure(
    CellB& cell,
    uint32_t x,
    uint32_t y,
    int targetX,
    int targetY,
    bool shouldTransferX,
    bool shouldTransferY,
    WorldB& world)
{
    // Transfer system disabled for now
}

void RulesBNew::handleBoundaryReflection(
    CellB& cell, int targetX, int targetY, bool shouldTransferX, bool shouldTransferY, WorldB& world)
{
    // Transfer system disabled for now
}

void RulesBNew::checkExcessiveDeflectionReflection(CellB& cell, WorldB& world)
{
    // Transfer system disabled for now
}

void RulesBNew::handleCollision(
    CellB& cell,
    uint32_t x,
    uint32_t y,
    int targetX,
    int targetY,
    bool shouldTransferX,
    bool shouldTransferY,
    const WorldB& world)
{
    // Transfer system disabled for now
}

Vector2d RulesBNew::calculateNaturalCOM(const Vector2d& currentCom, int deltaX, int deltaY) const
{
    // Transfer system disabled for now
    return Vector2d(0.0, 0.0);
}

Vector2d RulesBNew::clampCOMToDeadZone(const Vector2d& com) const
{
    // Transfer system disabled for now
    return Vector2d(0.0, 0.0);
}