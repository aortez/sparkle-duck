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
    switch (pressureSystem_) {
        case PressureSystem::TopDown:
            updatePressuresTopDown(world, deltaTimeSeconds);
            break;
        case PressureSystem::IterativeSettling:
            updatePressuresIterativeSettling(world, deltaTimeSeconds);
            break;
        case PressureSystem::Original:
        default:
            updatePressuresOriginal(world, deltaTimeSeconds);
            break;
    }
}

void RulesA::updatePressuresOriginal(World& world, double deltaTimeSeconds)
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

void RulesA::updatePressuresTopDown(World& world, double deltaTimeSeconds)
{
    // Clear all pressures first
    for (uint32_t y = 0; y < world.getHeight(); ++y) {
        for (uint32_t x = 0; x < world.getWidth(); ++x) {
            world.at(x, y).pressure = Vector2d(0.0, 0.0);
        }
    }

    spdlog::trace("=== TOP-DOWN PRESSURE GENERATION ===");

    // Process each column from top to bottom
    for (uint32_t x = 0; x < world.getWidth(); ++x) {
        double accumulatedMass = 0.0;

        for (uint32_t y = 0; y < world.getHeight(); ++y) {
            Cell& cell = world.at(x, y);

            // Add this cell's mass to the accumulated total
            if (cell.percentFull() >= World::MIN_DIRT_THRESHOLD) {
                accumulatedMass += cell.percentFull();
            }

            // Calculate hydrostatic pressure from accumulated mass above
            double hydrostaticPressure = accumulatedMass * gravity_ * deltaTimeSeconds * 0.1;

            // Apply base hydrostatic pressure downward
            cell.pressure.y += hydrostaticPressure;

            // Add lateral pressure based on COM deflection of all cells above
            Vector2d lateralPressure(0.0, 0.0);

            // Look at cells above to determine lateral pressure direction
            for (uint32_t checkY = 0; checkY <= y; ++checkY) {
                const Cell& upperCell = world.at(x, checkY);
                if (upperCell.percentFull() >= World::MIN_DIRT_THRESHOLD) {
                    Vector2d deflection = upperCell.getNormalizedDeflection();
                    // Scale lateral pressure by distance (closer cells have more influence)
                    double distanceScale = 1.0 / (1.0 + (y - checkY) * 0.5);
                    lateralPressure.x += deflection.x * upperCell.percentFull() * distanceScale;
                }
            }

            // Apply accumulated lateral pressure
            cell.pressure.x += lateralPressure.x * deltaTimeSeconds * 0.05;

            spdlog::trace(
                "Cell ({},{}) accumulated_mass={} hydrostatic_pressure={} lateral_pressure={}",
                x,
                y,
                accumulatedMass,
                hydrostaticPressure,
                lateralPressure.x);
        }
    }

    // Add horizontal pressure propagation
    for (uint32_t y = 0; y < world.getHeight(); ++y) {
        for (uint32_t x = 0; x < world.getWidth(); ++x) {
            const Cell& cell = world.at(x, y);
            if (cell.percentFull() < World::MIN_DIRT_THRESHOLD) continue;

            // Look at neighboring columns to determine pressure differences
            double leftPressure = (x > 0) ? world.at(x - 1, y).pressure.y : 0.0;
            double rightPressure = (x + 1 < world.getWidth()) ? world.at(x + 1, y).pressure.y : 0.0;
            double currentPressure = cell.pressure.y;

            // Calculate pressure gradients
            double leftGradient = currentPressure - leftPressure;
            double rightGradient = currentPressure - rightPressure;

            // Apply horizontal pressure based on gradients
            if (leftGradient > 0.001) {
                if (x > 0) {
                    world.at(x - 1, y).pressure.x += leftGradient * 0.1;
                }
            }
            if (rightGradient > 0.001) {
                if (x + 1 < world.getWidth()) {
                    world.at(x + 1, y).pressure.x += rightGradient * 0.1;
                }
            }
        }
    }
}

void RulesA::updatePressuresIterativeSettling(World& world, double deltaTimeSeconds)
{
    // Clear all pressures first
    for (uint32_t y = 0; y < world.getHeight(); ++y) {
        for (uint32_t x = 0; x < world.getWidth(); ++x) {
            world.at(x, y).pressure = Vector2d(0.0, 0.0);
        }
    }

    spdlog::trace("=== ITERATIVE SETTLING PRESSURE GENERATION ===");

    // Multiple settling passes - each pass allows material to settle under pressure
    const int numSettlingPasses = 3;
    const double passTimeStep = deltaTimeSeconds / numSettlingPasses;

    for (int pass = 0; pass < numSettlingPasses; ++pass) {
        spdlog::trace("Settling pass {}/{}", (pass + 1), numSettlingPasses);

        // For each pass, work from top to bottom
        for (uint32_t y = 0; y < world.getHeight(); ++y) {
            for (uint32_t x = 0; x < world.getWidth(); ++x) {
                Cell& cell = world.at(x, y);
                if (cell.percentFull() < World::MIN_DIRT_THRESHOLD) continue;

                // Calculate pressure from mass above (looking at previous pass results)
                double pressureFromAbove = 0.0;
                for (uint32_t checkY = 0; checkY < y; ++checkY) {
                    const Cell& upperCell = world.at(x, checkY);
                    if (upperCell.percentFull() >= World::MIN_DIRT_THRESHOLD) {
                        // Distance decay factor - closer cells contribute more pressure
                        double distanceFactor = 1.0 / (1.0 + (y - checkY) * 0.3);
                        pressureFromAbove += upperCell.percentFull() * gravity_ * distanceFactor;
                    }
                }

                // Apply settling pressure (increases with each pass)
                double settlingPressure = pressureFromAbove * passTimeStep * (pass + 1) * 0.02;
                cell.pressure.y += settlingPressure;

                // Add lateral pressure redistribution based on pressure differences
                Vector2d lateralPressure(0.0, 0.0);

                // Check neighboring columns for pressure differences
                if (x > 0) {
                    const Cell& leftCell = world.at(x - 1, y);
                    double pressureDiff = cell.pressure.y - leftCell.pressure.y;
                    if (pressureDiff > 0.001) {
                        lateralPressure.x -= pressureDiff * 0.1; // Pressure flows left
                        world.at(x - 1, y).pressure.x += pressureDiff * 0.1;
                    }
                }

                if (x + 1 < world.getWidth()) {
                    const Cell& rightCell = world.at(x + 1, y);
                    double pressureDiff = cell.pressure.y - rightCell.pressure.y;
                    if (pressureDiff > 0.001) {
                        lateralPressure.x += pressureDiff * 0.1; // Pressure flows right
                        world.at(x + 1, y).pressure.x += pressureDiff * 0.1;
                    }
                }

                cell.pressure.x += lateralPressure.x;

                // Add COM deflection influence (but scaled down since hydrostatic is primary)
                Vector2d deflection = cell.getNormalizedDeflection();
                cell.pressure.x += deflection.x * cell.percentFull() * passTimeStep * 0.02;
                cell.pressure.y += deflection.y * cell.percentFull() * passTimeStep * 0.02;

                spdlog::trace(
                    "Pass {} Cell ({},{}) pressure_from_above={} settling_pressure={} "
                    "final_pressure=({},{})",
                    pass,
                    x,
                    y,
                    pressureFromAbove,
                    settlingPressure,
                    cell.pressure.x,
                    cell.pressure.y);
            }
        }

        // Smooth pressure between passes to prevent oscillations
        if (pass < numSettlingPasses - 1) {
            std::vector<Vector2d> smoothedPressure(world.getWidth() * world.getHeight());
            for (uint32_t y = 0; y < world.getHeight(); ++y) {
                for (uint32_t x = 0; x < world.getWidth(); ++x) {
                    Vector2d sum = world.at(x, y).pressure;
                    int count = 1;

                    // Average with neighbors for smoothing
                    for (int dy = -1; dy <= 1; ++dy) {
                        for (int dx = -1; dx <= 1; ++dx) {
                            if (dx == 0 && dy == 0) continue;
                            int nx = x + dx, ny = y + dy;
                            if (nx >= 0 && nx < (int)world.getWidth() && ny >= 0
                                && ny < (int)world.getHeight()) {
                                sum +=
                                    world.at(nx, ny).pressure * 0.3; // Reduced weight for neighbors
                                count++;
                            }
                        }
                    }
                    smoothedPressure[y * world.getWidth() + x] = sum / count;
                }
            }

            // Apply smoothed pressure back to cells
            for (uint32_t y = 0; y < world.getHeight(); ++y) {
                for (uint32_t x = 0; x < world.getWidth(); ++x) {
                    world.at(x, y).pressure = smoothedPressure[y * world.getWidth() + x];
                }
            }
        }
    }
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