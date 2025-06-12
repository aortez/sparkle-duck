#include "WorldRules.h"
#include "World.h"
#include "spdlog/spdlog.h"

#include <algorithm>
#include <cmath>
#include <random>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Helper methods for WorldRules base class
bool WorldRules::isWithinBounds(int x, int y, const World& world)
{
    return x >= 0 && x < static_cast<int>(world.getWidth()) && y >= 0
        && y < static_cast<int>(world.getHeight());
}

Vector2d WorldRules::calculateNaturalCOM(const Vector2d& sourceCOM, int deltaX, int deltaY)
{
    Vector2d naturalCOM = sourceCOM;

    // If moving right (deltaX = 1), COM enters from the left side.
    if (deltaX > 0) {
        naturalCOM.x -= World::COM_CELL_WIDTH;
    }
    // If moving left (deltaX = -1), COM enters from the right side.
    else if (deltaX < 0) {
        naturalCOM.x += World::COM_CELL_WIDTH;
    }

    // If moving down (deltaY = 1), COM enters from the top side.
    if (deltaY > 0) {
        naturalCOM.y -= World::COM_CELL_WIDTH;
    }
    // If moving up (deltaY = -1), COM enters from the bottom side.
    else if (deltaY < 0) {
        naturalCOM.y += World::COM_CELL_WIDTH;
    }

    return naturalCOM;
}

Vector2d WorldRules::clampCOMToDeadZone(const Vector2d& naturalCOM)
{
    return Vector2d(
        std::max(
            -Cell::COM_DEFLECTION_THRESHOLD,
            std::min(Cell::COM_DEFLECTION_THRESHOLD, naturalCOM.x)),
        std::max(
            -Cell::COM_DEFLECTION_THRESHOLD,
            std::min(Cell::COM_DEFLECTION_THRESHOLD, naturalCOM.y)));
}

// RulesA Implementation
RulesA::RulesA()
{
    spdlog::info("Initialized RulesA physics rules");
}

void RulesA::applyPhysics(
    Cell& cell, uint32_t x, uint32_t y, double deltaTimeSeconds, const World& world)
{
    // Debug: Log initial state
    if (cell.v.x != 0.0 || cell.v.y != 0.0) {
        spdlog::trace(
            "Cell ({},{}) initial state: v=({},{}), com=({},{})",
            x,
            y,
            cell.v.x,
            cell.v.y,
            cell.com.x,
            cell.com.y);
    }

    // Apply gravity
    cell.v.y += gravity_ * deltaTimeSeconds;

    // Apply water physics (cohesion, viscosity) and buoyancy
    for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            if (dx == 0 && dy == 0) continue;

            int nx = x + dx;
            int ny = y + dy;
            if (isWithinBounds(nx, ny, world)) {
                const Cell& neighbor = world.at(nx, ny);

                // Apply water cohesion and viscosity if this is a water cell
                if (cell.water >= World::MIN_DIRT_THRESHOLD) {
                    Vector2d cohesion = cell.calculateWaterCohesion(cell, neighbor, &world, x, y);
                    cell.v += cohesion * deltaTimeSeconds;
                    cell.applyViscosity(neighbor);
                }

                // Apply buoyancy forces (works on any cell with dirt or water)
                Vector2d buoyancy = cell.calculateBuoyancy(cell, neighbor, dx, dy);
                cell.v += buoyancy * deltaTimeSeconds;
            }
        }
    }

    // Apply cursor force if active (this might need to be moved to World if shared)
    // For now, we'll skip cursor force implementation in rules and let World handle it
}

void RulesA::updatePressures(World& world, double deltaTimeSeconds)
{
    // First clear all pressures.
    for (uint32_t y = 0; y < world.getHeight(); ++y) {
        for (uint32_t x = 0; x < world.getWidth(); ++x) {
            world.at(x, y).pressure = Vector2d(0.0, 0.0);
        }
    }

    spdlog::trace("=== PRESSURE GENERATION PHASE ===");
    int pressuresGenerated = 0;

    // Then calculate pressures that cells exert on their neighbor.
    for (uint32_t y = 0; y < world.getHeight(); ++y) {
        for (uint32_t x = 0; x < world.getWidth(); ++x) {
            const Cell& cell = world.at(x, y);
            if (cell.percentFull() < World::MIN_DIRT_THRESHOLD) continue;
            double mass = cell.percentFull();

            // Get normalized deflection in [-1, 1] range
            Vector2d normalizedDeflection = cell.getNormalizedDeflection();

            if (normalizedDeflection.mag() > 0.01) {
                spdlog::trace(
                    "Cell ({},{}) deflection=({},{}) mag={}",
                    x,
                    y,
                    normalizedDeflection.x,
                    normalizedDeflection.y,
                    normalizedDeflection.mag());
            }

            // Right neighbor
            if (x + 1 < world.getWidth()) {
                if (normalizedDeflection.x > 0.0) {
                    Cell& neighbor = world.at(x + 1, y);
                    double pressureAdded = normalizedDeflection.x * mass * deltaTimeSeconds;
                    neighbor.pressure.x += pressureAdded;
                    spdlog::trace(
                        "Adding pressure {} to right neighbor ({},{})", pressureAdded, (x + 1), y);
                    pressuresGenerated++;
                }
            }
            // Left neighbor
            if (x > 0) {
                if (normalizedDeflection.x < 0.0) {
                    Cell& neighbor = world.at(x - 1, y);
                    double pressureAdded = -normalizedDeflection.x * mass * deltaTimeSeconds;
                    neighbor.pressure.x += pressureAdded;
                    spdlog::trace(
                        "Adding pressure {} to left neighbor ({},{})", pressureAdded, (x - 1), y);
                    pressuresGenerated++;
                }
            }
            // Down neighbor
            if (y + 1 < world.getHeight()) {
                if (normalizedDeflection.y > 0.0) {
                    Cell& neighbor = world.at(x, y + 1);
                    double pressureAdded = normalizedDeflection.y * mass * deltaTimeSeconds;
                    neighbor.pressure.y += pressureAdded;
                    spdlog::trace(
                        "Adding pressure {} to down neighbor ({},{})", pressureAdded, x, (y + 1));
                    pressuresGenerated++;
                }
            }
            // Up neighbor
            if (y > 0) {
                if (normalizedDeflection.y < 0.0) {
                    Cell& neighbor = world.at(x, y - 1);
                    double pressureAdded = -normalizedDeflection.y * mass * deltaTimeSeconds;
                    neighbor.pressure.y += pressureAdded;
                    spdlog::trace(
                        "Adding pressure {} to up neighbor ({},{})", pressureAdded, x, (y - 1));
                    pressuresGenerated++;
                }
            }
        }
    }

    spdlog::trace("Generated {} pressure contributions this frame", pressuresGenerated);
}


void RulesA::applyPressureForces(World& world, double deltaTimeSeconds)
{
    // This method would contain the pressure application logic from World::applyPressure
    // For now, we'll keep a simplified version
    // The full implementation would be extracted from the World class

    static std::mt19937 rng(std::random_device{}());
    static std::uniform_real_distribution<double> dist(0.0, 1.0);

    int pressureApplications = 0;

    for (uint32_t y = 0; y < world.getHeight(); y++) {
        for (uint32_t x = 0; x < world.getWidth(); x++) {
            Cell& cell = world.at(x, y);

            // Skip empty cells or cells with negligible pressure
            if (cell.percentFull() < World::MIN_DIRT_THRESHOLD) continue;
            if (cell.pressure.mag() < 0.001) continue;

            // Calculate pressure thresholds based on material type
            double pressureThreshold;
            if (cell.water > cell.dirt) {
                pressureThreshold = waterPressureThreshold_;
            }
            else {
                pressureThreshold = 0.005;
            }

            double pressureMagnitude = cell.pressure.mag();
            if (pressureMagnitude < pressureThreshold) continue;

            // Apply simplified pressure force
            Vector2d pressureForce = cell.pressure.normalize() * pressureMagnitude * pressureScale_;
            cell.v += pressureForce * deltaTimeSeconds;
            pressureApplications++;

            // Cap velocity to prevent explosive movement
            const double MAX_PRESSURE_VELOCITY = (cell.water > cell.dirt) ? 4.0 : 8.0;
            if (cell.v.mag() > MAX_PRESSURE_VELOCITY) {
                cell.v = cell.v.normalize() * MAX_PRESSURE_VELOCITY;
            }
        }
    }

    if (pressureApplications > 0) {
        spdlog::trace("Applied pressure to {} cells this frame", pressureApplications);
    }
}

bool RulesA::shouldTransfer(const Cell& cell, uint32_t x, uint32_t y, const World& world) const
{
    // Check if COM is outside deflection threshold
    return std::abs(cell.com.x) > Cell::COM_DEFLECTION_THRESHOLD
        || std::abs(cell.com.y) > Cell::COM_DEFLECTION_THRESHOLD;
}

void RulesA::calculateTransferDirection(
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
    shouldTransferX = false;
    shouldTransferY = false;
    targetX = x;
    targetY = y;
    comOffset = Vector2d(0.0, 0.0);

    // Check horizontal transfer based on COM deflection
    if (cell.com.x > Cell::COM_DEFLECTION_THRESHOLD) {
        shouldTransferX = true;
        targetX = x + 1;
        comOffset.x = clampCOMToDeadZone(calculateNaturalCOM(Vector2d(cell.com.x, 0.0), 1, 0)).x;
        spdlog::trace("  Transfer right: com.x={}, target_com.x={}", cell.com.x, comOffset.x);
    }
    else if (cell.com.x < -Cell::COM_DEFLECTION_THRESHOLD) {
        shouldTransferX = true;
        targetX = x - 1;
        comOffset.x = clampCOMToDeadZone(calculateNaturalCOM(Vector2d(cell.com.x, 0.0), -1, 0)).x;
        spdlog::trace("  Transfer left: com.x={}, target_com.x={}", cell.com.x, comOffset.x);
    }

    // Check vertical transfer based on COM deflection
    if (cell.com.y > Cell::COM_DEFLECTION_THRESHOLD) {
        shouldTransferY = true;
        targetY = y + 1;
        comOffset.y = clampCOMToDeadZone(calculateNaturalCOM(Vector2d(0.0, cell.com.y), 0, 1)).y;
        spdlog::trace("  Transfer down: com.y={}, target_com.y={}", cell.com.y, comOffset.y);
    }
    else if (cell.com.y < -Cell::COM_DEFLECTION_THRESHOLD) {
        shouldTransferY = true;
        targetY = y - 1;
        comOffset.y = clampCOMToDeadZone(calculateNaturalCOM(Vector2d(0.0, cell.com.y), 0, -1)).y;
        spdlog::trace("  Transfer up: com.y={}, target_com.y={}", cell.com.y, comOffset.y);
    }
}

bool RulesA::attemptTransfer(
    Cell& cell,
    uint32_t x,
    uint32_t y,
    int targetX,
    int targetY,
    const Vector2d& comOffset,
    double totalMass,
    World& world)
{
    if (!isWithinBounds(targetX, targetY, world)) {
        return false;
    }

    Cell& targetCell = world.at(targetX, targetY);
    if (targetCell.percentFull() >= 1.0) {
        spdlog::trace("  Transfer blocked by full cell at ({},{})", targetX, targetY);
        return false;
    }

    // Calculate transfer amounts
    const double availableSpace = 1.0 - targetCell.percentFull();
    const double safeAvailableSpace =
        std::max(0.0, availableSpace - 0.01); // Leave 1% safety margin
    const double moveAmount = std::min(totalMass, safeAvailableSpace * World::TRANSFER_FACTOR);

    // Don't transfer if move amount is zero
    if (moveAmount <= 0.0) {
        return false;
    }

    const double dirtProportion = cell.dirt / totalMass;
    const double waterProportion = cell.water / totalMass;
    const double dirtAmount = moveAmount * dirtProportion;
    const double waterAmount = moveAmount * waterProportion;

    world.moves.push_back(DirtMove{ .fromX = x,
                              .fromY = y,
                              .toX = static_cast<uint32_t>(targetX),
                              .toY = static_cast<uint32_t>(targetY),
                              .dirtAmount = dirtAmount,
                              .waterAmount = waterAmount,
                              .comOffset = comOffset });

    spdlog::trace(
        "  Queued move: from=({},{}) to=({},{}), dirt={}, water={}",
        x,
        y,
        targetX,
        targetY,
        dirtAmount,
        waterAmount);
    return true;
}

void RulesA::handleTransferFailure(
    Cell& cell,
    uint32_t x,
    uint32_t y,
    int targetX,
    int targetY,
    bool shouldTransferX,
    bool shouldTransferY,
    World& world)
{
    // Comprehensive transfer failure handling that conserves momentum

    // First, handle boundary collisions (out of bounds)
    if (shouldTransferX && !isWithinBounds(targetX, y, world)) {
        // Horizontal boundary collision
        cell.v.x = -cell.v.x * elasticityFactor_;
        cell.com.x =
            (targetX < 0) ? -Cell::COM_DEFLECTION_THRESHOLD : Cell::COM_DEFLECTION_THRESHOLD;
        spdlog::trace("  X boundary reflection: COM.x={}, v.x={}", cell.com.x, cell.v.x);
    }

    if (shouldTransferY && !isWithinBounds(x, targetY, world)) {
        // Vertical boundary collision
        cell.v.y = -cell.v.y * elasticityFactor_;
        cell.com.y =
            (targetY < 0) ? -Cell::COM_DEFLECTION_THRESHOLD : Cell::COM_DEFLECTION_THRESHOLD;
        spdlog::trace("  Y boundary reflection: COM.y={}, v.y={}", cell.com.y, cell.v.y);
    }

    // Handle in-bounds collisions (blocked by other cells)
    bool hitHorizontalObstacle = false;
    bool hitVerticalObstacle = false;

    if (shouldTransferX && isWithinBounds(targetX, y, world)) {
        double targetFullness = world.at(targetX, y).percentFull();
        if (targetFullness >= 0.95) { // Nearly full cell blocks transfer
            hitHorizontalObstacle = true;
            cell.v.x = -cell.v.x * elasticityFactor_;
            cell.com.x =
                (cell.com.x > 0) ? Cell::COM_DEFLECTION_THRESHOLD : -Cell::COM_DEFLECTION_THRESHOLD;
            spdlog::trace(
                "  X collision with full cell ({},{}): fullness={}", targetX, y, targetFullness);
        }
        else if (targetFullness > 0.7) {
            // Partial blockage - reduce momentum without full reflection
            double blockageFactor = (targetFullness - 0.7) / 0.25; // 0.0 to 1.0
            cell.v.x *= (1.0 - blockageFactor * 0.5);              // Reduce by up to 50%
            spdlog::trace(
                "  X partial blockage at ({},{}): fullness={}, v.x reduced to {}",
                targetX,
                y,
                targetFullness,
                cell.v.x);
        }
    }

    if (shouldTransferY && isWithinBounds(x, targetY, world)) {
        double targetFullness = world.at(x, targetY).percentFull();
        if (targetFullness >= 0.95) {
            hitVerticalObstacle = true;
            cell.v.y = -cell.v.y * elasticityFactor_;
            cell.com.y =
                (cell.com.y > 0) ? Cell::COM_DEFLECTION_THRESHOLD : -Cell::COM_DEFLECTION_THRESHOLD;
            spdlog::trace(
                "  Y collision with full cell ({},{}): fullness={}", x, targetY, targetFullness);
        }
        else if (targetFullness > 0.7) {
            // Partial blockage - reduce momentum without full reflection
            double blockageFactor = (targetFullness - 0.7) / 0.25; // 0.0 to 1.0
            cell.v.y *= (1.0 - blockageFactor * 0.5);              // Reduce by up to 50%
            spdlog::trace(
                "  Y partial blockage at ({},{}): fullness={}, v.y reduced to {}",
                x,
                targetY,
                targetFullness,
                cell.v.y);
        }
    }

    // Monte Carlo transfer failure - momentum bleeding to prevent buildup
    // This handles the case where transfer was blocked not due to physics, but due to Monte Carlo
    // selection
    if (!hitHorizontalObstacle && !hitVerticalObstacle && (shouldTransferX || shouldTransferY)) {
        // Apply gentle momentum damping to prevent infinite acceleration
        const double MOMENTUM_BLEED_FACTOR = 0.02; // 2% velocity reduction per failed transfer

        if (shouldTransferX && isWithinBounds(targetX, y, world) && world.at(targetX, y).percentFull() < 0.7) {
            // Transfer could have succeeded but was blocked by Monte Carlo - apply momentum bleed
            cell.v.x *= (1.0 - MOMENTUM_BLEED_FACTOR);
            cell.com.x *= (1.0 - MOMENTUM_BLEED_FACTOR * 0.5);
            spdlog::trace("  Monte Carlo X momentum bleed: new v.x={}", cell.v.x);
        }

        if (shouldTransferY && isWithinBounds(x, targetY, world) && world.at(x, targetY).percentFull() < 0.7) {
            // Transfer could have succeeded but was blocked by Monte Carlo - apply momentum bleed
            cell.v.y *= (1.0 - MOMENTUM_BLEED_FACTOR);
            cell.com.y *= (1.0 - MOMENTUM_BLEED_FACTOR * 0.5);
            spdlog::trace("  Monte Carlo Y momentum bleed: new v.y={}", cell.v.y);
        }
    }
}

void RulesA::handleBoundaryReflection(
    Cell& cell, int targetX, int targetY, bool shouldTransferX, bool shouldTransferY, World& world)
{
    // Legacy method - now handled by handleTransferFailure
    // Kept for compatibility but functionality moved to handleTransferFailure
    handleTransferFailure(cell, 0, 0, targetX, targetY, shouldTransferX, shouldTransferY, world);
}

void RulesA::checkExcessiveDeflectionReflection(Cell& cell, World& world)
{
    Vector2d normalizedDeflection = cell.getNormalizedDeflection();

    if (std::abs(normalizedDeflection.x) > World::REFLECTION_THRESHOLD) {
        cell.v.x = -cell.v.x * elasticityFactor_;
        cell.com.x = (normalizedDeflection.x > 0) ? Cell::COM_DEFLECTION_THRESHOLD
                                                  : -Cell::COM_DEFLECTION_THRESHOLD;
        spdlog::trace("  Horizontal reflection: COM.x={}, v.x={}", cell.com.x, cell.v.x);
    }

    if (std::abs(normalizedDeflection.y) > World::REFLECTION_THRESHOLD) {
        cell.v.y = -cell.v.y * elasticityFactor_;
        cell.com.y = (normalizedDeflection.y > 0) ? Cell::COM_DEFLECTION_THRESHOLD
                                                  : -Cell::COM_DEFLECTION_THRESHOLD;
        spdlog::trace("  Vertical reflection: COM.y={}, v.y={}", cell.com.y, cell.v.y);
    }
}

void RulesA::handleCollision(
    Cell& cell,
    uint32_t x,
    uint32_t y,
    int targetX,
    int targetY,
    bool shouldTransferX,
    bool shouldTransferY,
    const World& world)
{
    // Simplified collision handling - full implementation would be extracted from World

    // Handle boundary collisions
    if (shouldTransferX && !isWithinBounds(targetX, y, world)) {
        cell.v.x = -cell.v.x * elasticityFactor_;
        cell.com.x =
            (targetX < 0) ? -Cell::COM_DEFLECTION_THRESHOLD : Cell::COM_DEFLECTION_THRESHOLD;
        spdlog::trace("  X boundary reflection: COM.x={}, v.x={}", cell.com.x, cell.v.x);
    }

    if (shouldTransferY && !isWithinBounds(x, targetY, world)) {
        cell.v.y = -cell.v.y * elasticityFactor_;
        cell.com.y =
            (targetY < 0) ? -Cell::COM_DEFLECTION_THRESHOLD : Cell::COM_DEFLECTION_THRESHOLD;
        spdlog::trace("  Y boundary reflection: COM.y={}, v.y={}", cell.com.y, cell.v.y);
    }

    // Handle in-bounds collisions with other cells
    if (shouldTransferX && isWithinBounds(targetX, y, world)) {
        double targetFullness = world.at(targetX, y).percentFull();
        if (targetFullness >= 0.95) {
            cell.v.x = -cell.v.x * elasticityFactor_;
            cell.com.x =
                (cell.com.x > 0) ? Cell::COM_DEFLECTION_THRESHOLD : -Cell::COM_DEFLECTION_THRESHOLD;
            spdlog::trace(
                "  X collision with full cell ({},{}): fullness={}", targetX, y, targetFullness);
        }
    }

    if (shouldTransferY && isWithinBounds(x, targetY, world)) {
        double targetFullness = world.at(x, targetY).percentFull();
        if (targetFullness >= 0.95) {
            cell.v.y = -cell.v.y * elasticityFactor_;
            cell.com.y =
                (cell.com.y > 0) ? Cell::COM_DEFLECTION_THRESHOLD : -Cell::COM_DEFLECTION_THRESHOLD;
            spdlog::trace(
                "  Y collision with full cell ({},{}): fullness={}", x, targetY, targetFullness);
        }
    }
}

// Factory function
std::unique_ptr<WorldRules> createWorldRules(const std::string& rulesType)
{
    if (rulesType == "RulesA") {
        return std::make_unique<RulesA>();
    }

    // Default to RulesA
    spdlog::warn("Unknown rules type '{}', defaulting to RulesA", rulesType);
    return std::make_unique<RulesA>();
}