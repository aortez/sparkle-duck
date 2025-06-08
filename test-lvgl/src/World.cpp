#include "World.h"
#include "Cell.h"
#include "SimulatorUI.h"
#include "Timers.h"
#include "WorldSetup.h"
#include <cassert>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#include <algorithm>
#include <iostream>
#include <random>
#include <stdexcept>

#define LOG_DEBUG
#ifdef LOG_DEBUG
#define LOG_DEBUG(x) std::cout << x << std::endl
#else
#define LOG_DEBUG(x) ((void)0)
#endif

// Debug logging specifically for particle events
#define LOG_PARTICLES
#ifdef LOG_PARTICLES
#define LOG_PARTICLES(x) std::cout << "[Particles] " << x << std::endl
#else
#define LOG_PARTICLES(x) ((void)0)
#endif

// Define ASSERT macro if not already defined
#ifndef ASSERT
#define ASSERT(cond, msg, ...) assert(cond)
#endif

namespace {
// Track the next expected time for each event
struct EventState {
    double nextTopDrop = 0.33;       // First top drop at 0.33s
    double nextInitialThrow = 0.17;  // First throw at 0.17s
    double nextPeriodicThrow = 0.83; // First periodic throw at 0.83s
    double nextRightThrow = 1.0;     // First right throw at 1.0s
    double sweepTime = 0.0;          // Time tracking for sweep
    double beatTime = 0.0;           // Time tracking for beats
    bool initialThrowDone = false;   // Track if initial throw has happened
    bool topDropDone = false;        // Track if top drop has happened
};
} // namespace

// Initialize static member variables
double World::ELASTICITY_FACTOR = 0.8;         // Energy preserved in reflections (0.0 to 1.0)
double World::DIRT_FRAGMENTATION_FACTOR = 0.0; // Default dirt fragmentation factor

World::World(uint32_t width, uint32_t height, lv_obj_t* draw_area)
    : width(width), height(height), cells(width * height), draw_area(draw_area)
{
    // Initialize timers
    timers.startTimer("total_simulation");

    // Create configurable world setup
    worldSetup = std::make_unique<ConfigurableWorldSetup>();
}

World::~World()
{
    // Stop the total simulation timer and dump stats
    timers.stopTimer("total_simulation");
    timers.dumpTimerStats();
}

void World::advanceTime(const double deltaTimeSeconds)
{
    timers.startTimer("advance_time");

    // Update simulation time
    simulationTime += deltaTimeSeconds;

    // Save state if time reversal is enabled and conditions are met
    if (timeReversalEnabled) {
        bool shouldSave = false;

        // Save on user input
        if (hasUserInputSinceLastSave) {
            shouldSave = true;
        }

        // Save every 500ms (periodic fallback)
        if (simulationTime - lastSaveTime >= PERIODIC_SAVE_INTERVAL) {
            shouldSave = true;
        }

        if (shouldSave) {
            saveWorldState();
            hasUserInputSinceLastSave = false;
            lastSaveTime = simulationTime;
        }
    }

    processParticleAddition(deltaTimeSeconds);

    processDragEnd();

    updateAllPressures(deltaTimeSeconds);

    applyPressure(deltaTimeSeconds);

    processTransfers(deltaTimeSeconds);

    applyMoves(); // Apply the pressure moves

    updateTotalMass();

    timers.stopTimer("advance_time");
}

void World::processParticleAddition(double deltaTimeSeconds)
{
    if (addParticlesEnabled) {
        timers.startTimer("add_particles");
        worldSetup->addParticles(*this, timestep++, deltaTimeSeconds);
        timers.stopTimer("add_particles");
    }
    else {
        timestep++;
    }
}

void World::processDragEnd()
{
    if (pendingDragEnd.hasPendingEnd) {
        timers.startTimer("process_drag_end");
        if (pendingDragEnd.cellX >= 0 && pendingDragEnd.cellX < static_cast<int>(width)
            && pendingDragEnd.cellY >= 0 && pendingDragEnd.cellY < static_cast<int>(height)) {
            Cell& cell = at(pendingDragEnd.cellX, pendingDragEnd.cellY);
            cell.update(pendingDragEnd.dirt, pendingDragEnd.com, pendingDragEnd.velocity);

            LOG_DEBUG(
                "Processed drag end at (" << pendingDragEnd.cellX << "," << pendingDragEnd.cellY
                                          << ") with velocity (" << cell.v.x << "," << cell.v.y
                                          << ") and COM (" << cell.com.x << "," << cell.com.y
                                          << ")");
        }
        pendingDragEnd.hasPendingEnd = false;
        timers.stopTimer("process_drag_end");
    }
}

void World::processTransfers(double deltaTimeSeconds)
{
    // Collect potential moves
    for (uint32_t y = 0; y < height; y++) {
        for (uint32_t x = 0; x < width; x++) {
            // Skip the cell being dragged
            if (isDragging && static_cast<int>(x) == lastDragCellX
                && static_cast<int>(y) == lastDragCellY) {
                continue;
            }
            Cell& cell = at(x, y);

            // Skip empty cells or cells with mass below threshold
            if (cell.percentFull() < MIN_DIRT_THRESHOLD) {
                removedMass += cell.percentFull();
                cell.update(0.0, Vector2d(0.0, 0.0), Vector2d(0.0, 0.0));
                cell.water = 0.0;
                continue;
            }

            applyPhysicsToCell(cell, x, y, deltaTimeSeconds);

            // Calculate predicted COM position after this timestep
            const Vector2d predictedCom = cell.com + cell.v * deltaTimeSeconds;

            // Check for transfers based on COM deflection
            bool shouldTransferX, shouldTransferY;
            int targetX, targetY;
            Vector2d comOffset;
            calculateTransferDirection(
                cell, shouldTransferX, shouldTransferY, targetX, targetY, comOffset, x, y);

            bool transferOccurred = false;
            if (shouldTransferX || shouldTransferY) {
                const double totalMass = cell.dirt + cell.water;

                // Try diagonal transfer first if both X and Y are needed
                if (shouldTransferX && shouldTransferY) {
                    transferOccurred =
                        attemptTransfer(cell, x, y, targetX, targetY, comOffset, totalMass);
                    if (!transferOccurred && !isWithinBounds(targetX, targetY)) {
                        handleBoundaryReflection(
                            cell, targetX, targetY, shouldTransferX, shouldTransferY);
                        shouldTransferX = shouldTransferX && isWithinBounds(targetX, y);
                        shouldTransferY = shouldTransferY && isWithinBounds(x, targetY);
                    }
                }

                // Try X transfer if no diagonal transfer occurred
                if (shouldTransferX && !transferOccurred) {
                    Vector2d xComOffset = comOffset;
                    xComOffset.y = cell.com.y; // Keep original Y component
                    transferOccurred =
                        attemptTransfer(cell, x, y, targetX, y, xComOffset, totalMass);
                    if (!transferOccurred && !isWithinBounds(targetX, y)) {
                        handleBoundaryReflection(cell, targetX, y, true, false);
                    }
                }

                // Try Y transfer if needed and no transfer occurred
                if (shouldTransferY && !transferOccurred) {
                    Vector2d yComOffset = comOffset;
                    yComOffset.x = cell.com.x; // Keep original X component
                    transferOccurred =
                        attemptTransfer(cell, x, y, x, targetY, yComOffset, totalMass);
                    if (!transferOccurred && !isWithinBounds(x, targetY)) {
                        handleBoundaryReflection(cell, x, targetY, false, true);
                    }
                }
            }

            // If no transfer occurred, update COM internally
            if (!transferOccurred) {
                cell.update(cell.dirt, predictedCom, cell.v);
            }

            checkExcessiveDeflectionReflection(cell);
        }
    }
}

void World::applyPhysicsToCell(Cell& cell, uint32_t x, uint32_t y, double deltaTimeSeconds)
{
    // Debug: Log initial state
    if (cell.v.x != 0.0 || cell.v.y != 0.0) {
        LOG_DEBUG(
            "Cell (" << x << "," << y << ") initial state: v=(" << cell.v.x << "," << cell.v.y
                     << "), com=(" << cell.com.x << "," << cell.com.y << ")");
    }

    // Apply gravity
    cell.v.y += gravity * deltaTimeSeconds;

    // Apply water physics (cohesion, viscosity) and buoyancy
    for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            if (dx == 0 && dy == 0) continue;

            int nx = x + dx;
            int ny = y + dy;
            if (isWithinBounds(nx, ny)) {
                Cell& neighbor = at(nx, ny);

                // Apply water cohesion and viscosity if this is a water cell
                if (cell.water >= MIN_DIRT_THRESHOLD) {
                    Vector2d cohesion = cell.calculateWaterCohesion(cell, neighbor);
                    cell.v += cohesion * deltaTimeSeconds;
                    cell.applyViscosity(neighbor);
                }

                // Apply buoyancy forces (works on any cell with dirt or water)
                Vector2d buoyancy = cell.calculateBuoyancy(cell, neighbor, dx, dy);
                cell.v += buoyancy * deltaTimeSeconds;
            }
        }
    }

    // Apply cursor force if active
    if (cursorForceEnabled && cursorForceActive) {
        double dx = cursorForceX - static_cast<int>(x);
        double dy = cursorForceY - static_cast<int>(y);
        double distance = std::sqrt(dx * dx + dy * dy);

        if (distance <= CURSOR_FORCE_RADIUS) {
            double forceFactor = (1.0 - distance / CURSOR_FORCE_RADIUS) * CURSOR_FORCE_STRENGTH;
            if (distance > 0) {
                cell.v.x += (dx / distance) * forceFactor * deltaTimeSeconds;
                cell.v.y += (dy / distance) * forceFactor * deltaTimeSeconds;
            }
        }
    }
}

void World::calculateTransferDirection(
    const Cell& cell,
    bool& shouldTransferX,
    bool& shouldTransferY,
    int& targetX,
    int& targetY,
    Vector2d& comOffset,
    uint32_t x,
    uint32_t y)
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
        LOG_DEBUG("  Transfer right: com.x=" << cell.com.x << ", target_com.x=" << comOffset.x);
    }
    else if (cell.com.x < -Cell::COM_DEFLECTION_THRESHOLD) {
        shouldTransferX = true;
        targetX = x - 1;
        comOffset.x = clampCOMToDeadZone(calculateNaturalCOM(Vector2d(cell.com.x, 0.0), -1, 0)).x;
        LOG_DEBUG("  Transfer left: com.x=" << cell.com.x << ", target_com.x=" << comOffset.x);
    }

    // Check vertical transfer based on COM deflection
    if (cell.com.y > Cell::COM_DEFLECTION_THRESHOLD) {
        shouldTransferY = true;
        targetY = y + 1;
        comOffset.y = clampCOMToDeadZone(calculateNaturalCOM(Vector2d(0.0, cell.com.y), 0, 1)).y;
        LOG_DEBUG("  Transfer down: com.y=" << cell.com.y << ", target_com.y=" << comOffset.y);
    }
    else if (cell.com.y < -Cell::COM_DEFLECTION_THRESHOLD) {
        shouldTransferY = true;
        targetY = y - 1;
        comOffset.y = clampCOMToDeadZone(calculateNaturalCOM(Vector2d(0.0, cell.com.y), 0, -1)).y;
        LOG_DEBUG("  Transfer up: com.y=" << cell.com.y << ", target_com.y=" << comOffset.y);
    }
}

bool World::attemptTransfer(
    Cell& cell,
    uint32_t x,
    uint32_t y,
    int targetX,
    int targetY,
    const Vector2d& comOffset,
    double totalMass)
{
    if (!isWithinBounds(targetX, targetY)) {
        return false;
    }

    Cell& targetCell = at(targetX, targetY);
    if (targetCell.percentFull() >= 1.0) {
        LOG_DEBUG("  Transfer blocked by full cell at (" << targetX << "," << targetY << ")");
        return false;
    }

    // Calculate transfer amounts
    const double availableSpace = 1.0 - targetCell.percentFull();
    const double moveAmount = std::min(totalMass, availableSpace * TRANSFER_FACTOR);
    const double dirtProportion = cell.dirt / totalMass;
    const double waterProportion = cell.water / totalMass;
    const double dirtAmount = moveAmount * dirtProportion;
    const double waterAmount = moveAmount * waterProportion;

    moves.push_back(DirtMove{ .fromX = x,
                              .fromY = y,
                              .toX = static_cast<uint32_t>(targetX),
                              .toY = static_cast<uint32_t>(targetY),
                              .dirtAmount = dirtAmount,
                              .waterAmount = waterAmount,
                              .comOffset = comOffset });

    LOG_DEBUG(
        "  Queued move: from=(" << x << "," << y << ") to=(" << targetX << "," << targetY
                                << "), dirt=" << dirtAmount << ", water=" << waterAmount);
    return true;
}

void World::handleBoundaryReflection(
    Cell& cell, int targetX, int targetY, bool shouldTransferX, bool shouldTransferY)
{
    if (shouldTransferX && !isWithinBounds(targetX, 0)) {
        cell.v.x = -cell.v.x * ELASTICITY_FACTOR;
        cell.com.x =
            (targetX < 0) ? -Cell::COM_DEFLECTION_THRESHOLD : Cell::COM_DEFLECTION_THRESHOLD;
        LOG_DEBUG("  X boundary reflection: COM.x=" << cell.com.x << ", v.x=" << cell.v.x);
    }

    if (shouldTransferY && !isWithinBounds(0, targetY)) {
        cell.v.y = -cell.v.y * ELASTICITY_FACTOR;
        cell.com.y =
            (targetY < 0) ? -Cell::COM_DEFLECTION_THRESHOLD : Cell::COM_DEFLECTION_THRESHOLD;
        LOG_DEBUG("  Y boundary reflection: COM.y=" << cell.com.y << ", v.y=" << cell.v.y);
    }
}

void World::checkExcessiveDeflectionReflection(Cell& cell)
{
    Vector2d normalizedDeflection = cell.getNormalizedDeflection();

    if (std::abs(normalizedDeflection.x) > REFLECTION_THRESHOLD) {
        cell.v.x = -cell.v.x * ELASTICITY_FACTOR;
        cell.com.x = (normalizedDeflection.x > 0) ? Cell::COM_DEFLECTION_THRESHOLD
                                                  : -Cell::COM_DEFLECTION_THRESHOLD;
        LOG_DEBUG("  Horizontal reflection: COM.x=" << cell.com.x << ", v.x=" << cell.v.x);
    }

    if (std::abs(normalizedDeflection.y) > REFLECTION_THRESHOLD) {
        cell.v.y = -cell.v.y * ELASTICITY_FACTOR;
        cell.com.y = (normalizedDeflection.y > 0) ? Cell::COM_DEFLECTION_THRESHOLD
                                                  : -Cell::COM_DEFLECTION_THRESHOLD;
        LOG_DEBUG("  Vertical reflection: COM.y=" << cell.com.y << ", v.y=" << cell.v.y);
    }
}

bool World::isWithinBounds(int x, int y) const
{
    return x >= 0 && x < static_cast<int>(width) && y >= 0 && y < static_cast<int>(height);
}

Vector2d World::calculateNaturalCOM(const Vector2d& sourceCOM, int deltaX, int deltaY)
{
    return Vector2d(sourceCOM.x - deltaX * COM_CELL_WIDTH, sourceCOM.y - deltaY * COM_CELL_WIDTH);
}

Vector2d World::clampCOMToDeadZone(const Vector2d& naturalCOM)
{
    return Vector2d(
        std::max(
            -Cell::COM_DEFLECTION_THRESHOLD,
            std::min(Cell::COM_DEFLECTION_THRESHOLD, naturalCOM.x)),
        std::max(
            -Cell::COM_DEFLECTION_THRESHOLD,
            std::min(Cell::COM_DEFLECTION_THRESHOLD, naturalCOM.y)));
}

void World::updateTotalMass()
{
    totalMass = 0.0;
    for (uint32_t y = 0; y < height; y++) {
        for (uint32_t x = 0; x < width; x++) {
            totalMass += at(x, y).percentFull();
        }
    }

    // Update UI if available
    if (ui_) {
        ui_->updateMassLabel(totalMass);
    }
}

void World::setUI(std::unique_ptr<SimulatorUI> ui)
{
    ui_ = std::move(ui);
}

void World::applyPressure(const double deltaTimeSeconds)
{
    // Random number generator for probabilistic pressure application
    static std::mt19937 rng(std::random_device{}());
    static std::uniform_real_distribution<double> dist(0.0, 1.0);

    int pressureApplications = 0; // Debug counter

    for (uint32_t y = 0; y < height; y++) {
        for (uint32_t x = 0; x < width; x++) {
            Cell& cell = at(x, y);

            // Skip empty cells or cells with negligible pressure
            if (cell.percentFull() < MIN_DIRT_THRESHOLD) continue;

            // Debug: Log pressure values for non-empty cells
            if (cell.pressure.mag() > 0.001) {
                LOG_DEBUG(
                    "Cell (" << x << "," << y
                             << ") has pressure magnitude: " << cell.pressure.mag());
            }

            if (cell.pressure.mag() < 0.001) continue;

            // Calculate pressure thresholds based on material type
            double pressureThreshold;
            double maxPressureForFullProb;
            if (cell.water > cell.dirt) {
                // Water flows easily - very low threshold
                pressureThreshold = waterPressureThreshold; // Configurable water pressure threshold
                maxPressureForFullProb = pressureThreshold * 10.0; // 100% at 10x threshold
            }
            else {
                // Dirt is more solid - higher threshold but still reachable
                pressureThreshold = 0.01; // Low enough to be triggered by typical pressure values
                maxPressureForFullProb = pressureThreshold * 20.0; // 100% at 20x threshold
            }

            double pressureMagnitude = cell.pressure.mag();

            LOG_DEBUG(
                "Cell (" << x << "," << y << ") pressure=" << pressureMagnitude
                         << " threshold=" << pressureThreshold);

            // Skip if below threshold
            if (pressureMagnitude < pressureThreshold) continue;

            // TEMPORARILY DISABLE PROBABILISTIC APPLICATION FOR DEBUGGING
            // Calculate probability of pressure application
            // 0% at threshold, 100% at maxPressureForFullProb
            double excessPressure = pressureMagnitude - pressureThreshold;
            double maxExcess = maxPressureForFullProb - pressureThreshold;
            double probability = std::min(1.0, excessPressure / maxExcess);

            LOG_DEBUG("Cell (" << x << "," << y << ") probability=" << probability);

            // ALWAYS APPLY FOR DEBUGGING - COMMENT OUT THE PROBABILISTIC CHECK
            // if (dist(rng) > probability) continue;

            // Calculate desired pressure direction
            Vector2d desiredDirection = cell.pressure.normalize();
            double pressureForce = pressureMagnitude * pressureScale;

            LOG_DEBUG(
                "Cell (" << x << "," << y << ") desiredDirection=(" << desiredDirection.x << ","
                         << desiredDirection.y << ")");

            // First, try to apply pressure in the desired direction
            // Calculate which neighbor cell the pressure is pointing toward
            int targetDx = 0, targetDy = 0;
            if (desiredDirection.x > 0.5)
                targetDx = 1;
            else if (desiredDirection.x < -0.5)
                targetDx = -1;
            if (desiredDirection.y > 0.5)
                targetDy = 1;
            else if (desiredDirection.y < -0.5)
                targetDy = -1;

            int targetX = static_cast<int>(x) + targetDx;
            int targetY = static_cast<int>(y) + targetDy;

            LOG_DEBUG(
                "Cell (" << x << "," << y << ") targetCell=(" << targetX << "," << targetY << ")");

            // Check if the desired direction has available space
            bool canApplyDirectly = false;
            if (isWithinBounds(targetX, targetY)) {
                Cell& targetCell = at(targetX, targetY);
                LOG_DEBUG(
                    "Cell (" << x << "," << y
                             << ") targetCell percentFull=" << targetCell.percentFull());
                if (targetCell.percentFull() < 0.95) { // Leave some margin for "available space"
                    canApplyDirectly = true;
                }
            }
            else {
                LOG_DEBUG(
                    "Cell (" << x << "," << y
                             << ") targetCell out of bounds - applying pressure against boundary");
                // Out of bounds means pressure against boundary - still apply force
                canApplyDirectly = true;
            }

            Vector2d appliedDirection;
            double forceMultiplier = 1.0;

            if (canApplyDirectly) {
                // Apply pressure directly in desired direction
                appliedDirection = desiredDirection;
                LOG_DEBUG("Cell (" << x << "," << y << ") applying directly in desired direction");
            }
            else {
                LOG_DEBUG("Cell (" << x << "," << y << ") searching for alternative neighbor");
                // Find the nearest open neighbor and deflect toward it
                double bestAngle = M_PI; // Start with worst possible angle (180°)
                bool foundOpenNeighbor = false;
                const double MAX_DEFLECTION_ANGLE = M_PI / 2.0; // 90 degrees max deflection

                for (int dy = -1; dy <= 1; dy++) {
                    for (int dx = -1; dx <= 1; dx++) {
                        if (dx == 0 && dy == 0) continue;

                        int nx = static_cast<int>(x) + dx;
                        int ny = static_cast<int>(y) + dy;

                        if (isWithinBounds(nx, ny)) {
                            Cell& neighbor = at(nx, ny);
                            if (neighbor.percentFull() < 0.95) { // Has available space
                                // Calculate direction to this neighbor
                                Vector2d neighborDirection = Vector2d(dx, dy).normalize();

                                // Calculate angle between desired direction and neighbor direction
                                double dotProduct = desiredDirection.dot(neighborDirection);
                                double angle = std::acos(std::max(-1.0, std::min(1.0, dotProduct)));

                                LOG_DEBUG(
                                    "  Neighbor (" << nx << "," << ny << ") direction=("
                                                   << neighborDirection.x << ","
                                                   << neighborDirection.y << ") angle=" << angle);

                                // Only consider neighbors within reasonable deflection angle
                                if (angle <= MAX_DEFLECTION_ANGLE && angle < bestAngle) {
                                    bestAngle = angle;
                                    appliedDirection = neighborDirection;
                                    foundOpenNeighbor = true;
                                    LOG_DEBUG(
                                        "  New best neighbor: (" << nx << "," << ny
                                                                 << ") angle=" << angle);
                                }
                            }
                            else {
                                LOG_DEBUG(
                                    "  Neighbor (" << nx << "," << ny << ") is full ("
                                                   << neighbor.percentFull() << ")");
                            }
                        }
                        else {
                            LOG_DEBUG("  Neighbor (" << nx << "," << ny << ") out of bounds");
                        }
                    }
                }

                if (!foundOpenNeighbor) {
                    // No open neighbors within reasonable angle, can't apply pressure
                    LOG_DEBUG(
                        "Cell (" << x << "," << y << ") no suitable neighbors found within "
                                 << (MAX_DEFLECTION_ANGLE * 180.0 / M_PI) << " degrees");
                    continue;
                }

                // Scale force by cosine of deflection angle
                forceMultiplier = std::cos(bestAngle);
                // Ensure we don't apply negative force (angles > 90°)
                forceMultiplier = std::abs(forceMultiplier); // std::max(0.0, forceMultiplier);
                LOG_DEBUG(
                    "Cell (" << x << "," << y << ") bestAngle=" << bestAngle
                             << " forceMultiplier=" << forceMultiplier);
            }

            // Apply the pressure force as acceleration
            // Scale by the probability to make higher pressure more effective
            double effectiveForce = pressureForce * forceMultiplier * 20; // Reduced for debugging
            Vector2d pressureAcceleration = appliedDirection * effectiveForce;

            LOG_DEBUG(
                "APPLYING PRESSURE at (" << x << "," << y << ") force=" << effectiveForce
                                         << " direction=(" << appliedDirection.x << ","
                                         << appliedDirection.y << ")");

            cell.v += pressureAcceleration;
            pressureApplications++;

            // Apply velocity limiting to prevent excessive velocities
            const double MAX_PRESSURE_VELOCITY = 8.0;
            if (cell.v.mag() > MAX_PRESSURE_VELOCITY) {
                cell.v = cell.v.normalize() * MAX_PRESSURE_VELOCITY;
            }
        }
    }

    if (pressureApplications > 0) {
        LOG_DEBUG("Applied pressure to " << pressureApplications << " cells this frame");
    }
}

void World::updateAllPressures(double deltaTimeSeconds)
{
    // First clear all pressures.
    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            at(x, y).pressure = Vector2d(0.0, 0.0);
        }
    }

    LOG_DEBUG("=== PRESSURE GENERATION PHASE ===");
    int pressuresGenerated = 0;

    // Then calculate pressures that cells exert on their neighbor.
    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            Cell& cell = at(x, y);
            if (cell.percentFull() < MIN_DIRT_THRESHOLD) continue;
            double mass = cell.percentFull();

            // Get normalized deflection in [-1, 1] range
            Vector2d normalizedDeflection = cell.getNormalizedDeflection();

            if (normalizedDeflection.mag() > 0.01) {
                LOG_DEBUG(
                    "Cell (" << x << "," << y << ") deflection=(" << normalizedDeflection.x << ","
                             << normalizedDeflection.y << ") mag=" << normalizedDeflection.mag());
            }

            // Right neighbor
            if (x + 1 < width) {
                if (normalizedDeflection.x > 0.0) {
                    Cell& neighbor = at(x + 1, y);
                    double pressureAdded = normalizedDeflection.x * mass * deltaTimeSeconds;
                    neighbor.pressure.x += pressureAdded;
                    LOG_DEBUG(
                        "Adding pressure " << pressureAdded << " to right neighbor (" << (x + 1)
                                           << "," << y << ")");
                    pressuresGenerated++;
                }
            }
            // Left neighbor
            if (x > 0) {
                if (normalizedDeflection.x < 0.0) {
                    Cell& neighbor = at(x - 1, y);
                    double pressureAdded = -normalizedDeflection.x * mass * deltaTimeSeconds;
                    neighbor.pressure.x += pressureAdded;
                    LOG_DEBUG(
                        "Adding pressure " << pressureAdded << " to left neighbor (" << (x - 1)
                                           << "," << y << ")");
                    pressuresGenerated++;
                }
            }
            // Down neighbor
            if (y + 1 < height) {
                if (normalizedDeflection.y > 0.0) {
                    Cell& neighbor = at(x, y + 1);
                    double pressureAdded = normalizedDeflection.y * mass * deltaTimeSeconds;
                    neighbor.pressure.y += pressureAdded;
                    LOG_DEBUG(
                        "Adding pressure " << pressureAdded << " to down neighbor (" << x << ","
                                           << (y + 1) << ")");
                    pressuresGenerated++;
                }
            }
            // Up neighbor
            if (y > 0) {
                if (normalizedDeflection.y < 0.0) {
                    Cell& neighbor = at(x, y - 1);
                    double pressureAdded = -normalizedDeflection.y * mass * deltaTimeSeconds;
                    neighbor.pressure.y += pressureAdded;
                    LOG_DEBUG(
                        "Adding pressure " << pressureAdded << " to up neighbor (" << x << ","
                                           << (y - 1) << ")");
                    pressuresGenerated++;
                }
            }
        }
    }

    LOG_DEBUG("Generated " << pressuresGenerated << " pressure contributions this frame");

    // TEMPORARILY DISABLE PRESSURE DIFFUSION FOR DEBUGGING
    // The diffusion is averaging pressure values and preventing threshold-based application
    const int numPressureIterations = 0; // Was 8
    std::vector<Vector2d> nextPressure(width * height);
    for (int iter = 0; iter < numPressureIterations; ++iter) {
        for (uint32_t y = 0; y < height; ++y) {
            for (uint32_t x = 0; x < width; ++x) {
                Vector2d sum = at(x, y).pressure;
                int count = 1;
                for (int dy = -1; dy <= 1; ++dy) {
                    for (int dx = -1; dx <= 1; ++dx) {
                        if (dx == 0 && dy == 0) continue;
                        int nx = x + dx, ny = y + dy;
                        if (nx >= 0 && nx < (int)width && ny >= 0 && ny < (int)height) {
                            sum += at(nx, ny).pressure;
                            ++count;
                        }
                    }
                }
                nextPressure[y * width + x] = sum / count;
            }
        }
        // Copy nextPressure back to cells
        for (uint32_t y = 0; y < height; ++y) {
            for (uint32_t x = 0; x < width; ++x) {
                at(x, y).pressure = nextPressure[y * width + x];
            }
        }
    }
}

Cell& World::at(uint32_t x, uint32_t y)
{
    if (x >= width || y >= height) {
        LOG_DEBUG(
            "World::at: Attempted to access coordinates ("
            << x << "," << y << ") but world size is " << width << "x" << height);
        throw std::out_of_range("World::at: Coordinates out of range");
    }
    return cells[coordToIndex(x, y)];
}

const Cell& World::at(uint32_t x, uint32_t y) const
{
    if (x >= width || y >= height) {
        LOG_DEBUG(
            "World::at: Attempted to access coordinates ("
            << x << "," << y << ") but world size is " << width << "x" << height);
        throw std::out_of_range("World::at: Coordinates out of range");
    }
    return cells[coordToIndex(x, y)];
}

size_t World::coordToIndex(uint32_t x, uint32_t y) const
{
    return y * width + x;
}

void World::applyMoves()
{
    // Shuffle moves to avoid bias (which ones are applied or not).
    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(moves.begin(), moves.end(), g);

    // Second pass: apply valid moves.
    for (const auto& move : moves) {
        Cell& sourceCell = at(move.fromX, move.fromY);
        Cell& targetCell = at(move.toX, move.toY);

        LOG_DEBUG(
            "Transfer: from=(" << move.fromX << "," << move.fromY << ") to=(" << move.toX << ","
                               << move.toY << ") source_v=(" << sourceCell.v.x << ","
                               << sourceCell.v.y << ") target_v=(" << targetCell.v.x << ","
                               << targetCell.v.y << ")");

        // Calculate maximum possible transfer while respecting capacity
        const double availableSpace = 1.0 - targetCell.percentFull();
        const double totalMass = move.dirtAmount + move.waterAmount;
        const double moveAmount = std::min({
            totalMass,
            sourceCell.percentFull(), // Can't move more than we have. TODO: should take into
                                      // account water vs dirt
            availableSpace            // Can't exceed target capacity.
        });

        // --- ELASTIC COLLISION RESPONSE (only if both cells have mass) ---
        if (sourceCell.percentFull() > MIN_DIRT_THRESHOLD
            && targetCell.percentFull() > MIN_DIRT_THRESHOLD) {
            // Normal vector from source to target
            Vector2d normal =
                Vector2d((int)move.toX - (int)move.fromX, (int)move.toY - (int)move.fromY)
                    .normalize();
            if (normal.mag() > 0.0) {
                // Project velocities onto the normal
                double v1n = sourceCell.v.dot(normal);
                double v2n = targetCell.v.dot(normal);
                // Tangential components (unchanged)
                Vector2d v1t = sourceCell.v - normal * v1n;
                Vector2d v2t = targetCell.v - normal * v2n;
                // Masses (use total mass)
                double m1 = sourceCell.percentFull();
                double m2 = targetCell.percentFull();
                // 1D elastic collision formula (with elasticity factor)
                double elasticity = World::ELASTICITY_FACTOR;
                double v1n_after = (v1n * (m1 - m2) + 2 * m2 * v2n) / (m1 + m2);
                double v2n_after = (v2n * (m2 - m1) + 2 * m1 * v1n) / (m1 + m2);
                // Interpolate with elasticity
                v1n_after = v1n + (v1n_after - v1n) * elasticity;
                v2n_after = v2n + (v2n_after - v2n) * elasticity;
                // Update velocities
                sourceCell.v = v1t + normal * v1n_after;
                targetCell.v = v2t + normal * v2n_after;
            }
        }
        // --- END ELASTIC COLLISION RESPONSE ---

        // Calculate the fraction being moved.
        const double moveFraction = moveAmount / sourceCell.percentFull();

        // Calculate actual amounts to move based on proportions
        const double dirtProportion = sourceCell.dirt / sourceCell.percentFull();
        const double waterProportion = sourceCell.water / sourceCell.percentFull();

        // Apply fragmentation: reduce dirt transfer by fragmentation factor
        // Higher fragmentation factor means more dirt stays behind
        const double dirtMoveAmount =
            moveAmount * dirtProportion * (1.0 - DIRT_FRAGMENTATION_FACTOR);
        const double waterMoveAmount =
            moveAmount * waterProportion; // Water not affected by fragmentation

        const double actualDirt = dirtMoveAmount;
        const double actualWater = waterMoveAmount;

        // Update source cell
        sourceCell.update(sourceCell.dirt - actualDirt, sourceCell.com, sourceCell.v);
        sourceCell.water -= actualWater;

        // Update target cell
        const double oldTargetMass = targetCell.percentFull();
        Vector2d expectedCom = move.comOffset;

        // Update target cell's COM using weighted average
        if (oldTargetMass == 0.0) {
            // First mass in cell, use the expected COM and transfer velocity
            targetCell.update(targetCell.dirt + actualDirt, expectedCom, sourceCell.v);
            targetCell.water += actualWater;
        }
        else {
            // Weighted average of existing COM and the new COM based on mass.
            // This preserves both mass and energy in the system.
            const double newMass = actualDirt + actualWater;
            Vector2d newCom = (targetCell.com * oldTargetMass + expectedCom * newMass)
                / (oldTargetMass + newMass);
            targetCell.update(targetCell.dirt + actualDirt, newCom, targetCell.v);
            targetCell.water += actualWater;
        }

        // Update source cell's COM and velocity if any mass remains.
        if (sourceCell.percentFull() > 0.0) {
            // Preserve velocity if no collision occurred
            Vector2d newV = sourceCell.v;
            // Scale COM based on remaining mass
            const double remainingMass = sourceCell.dirt + sourceCell.water;
            const double moveFraction =
                (actualDirt + actualWater) / (sourceCell.dirt + sourceCell.water);
            Vector2d newCom = sourceCell.com * (1.0 - moveFraction);
            sourceCell.update(sourceCell.dirt, newCom, newV);
        }
        else {
            sourceCell.update(0.0, Vector2d(0.0, 0.0), Vector2d(0.0, 0.0));
            sourceCell.water = 0.0;
        }

        // Debug: Log final state
        LOG_DEBUG(
            "After transfer: source_v=(" << sourceCell.v.x << "," << sourceCell.v.y
                                         << ") target_v=(" << targetCell.v.x << ","
                                         << targetCell.v.y << ")");
    }

    moves.clear();
}

void World::draw()
{
    if (draw_area == nullptr) {
        return;
    }

    timers.startTimer("draw");
    for (uint32_t y = 0; y < height; y++) {
        for (uint32_t x = 0; x < width; x++) {
            at(x, y).draw(draw_area, x, y);
        }
    }
    timers.stopTimer("draw");
}

uint32_t World::getWidth() const
{
    return width;
}

uint32_t World::getHeight() const
{
    return height;
}

void World::reset()
{
    // Exit time reversal navigation mode to ensure we're working with current state
    currentHistoryIndex = -1;
    hasStoredCurrentState = false;

    // Mark user input for time reversal
    markUserInput();

    // Clear all cells
    for (auto& cell : cells) {
        cell.update(0.0, Vector2d(0.0, 0.0), Vector2d(0.0, 0.0));
        cell.water = 0.0;
    }

    // Use the world setup strategy to initialize the world
    worldSetup->setup(*this);
}

void World::addDirtAtPixel(int pixelX, int pixelY)
{
    // Mark user input for time reversal
    markUserInput();

    // Convert pixel coordinates to cell coordinates
    int cellX = pixelX / Cell::WIDTH;
    int cellY = pixelY / Cell::HEIGHT;

    // Check if coordinates are within bounds
    if (cellX >= 0 && cellX < static_cast<int>(width) && cellY >= 0
        && cellY < static_cast<int>(height)) {
        Cell& cell = at(cellX, cellY);
        cell.update(1.0, Vector2d(0.0, 0.0), Vector2d(0.0, 0.0));
        cell.markDirty();
    }
}

void World::addWaterAtPixel(int pixelX, int pixelY)
{
    // Mark user input for time reversal
    markUserInput();

    // Convert pixel coordinates to cell coordinates
    int cellX = pixelX / Cell::WIDTH;
    int cellY = pixelY / Cell::HEIGHT;

    // Check if coordinates are within bounds
    if (cellX >= 0 && cellX < static_cast<int>(width) && cellY >= 0
        && cellY < static_cast<int>(height)) {
        Cell& cell = at(cellX, cellY);
        // Keep existing dirt amount but add water
        cell.water = 1.0;
        cell.markDirty();
    }
}

void World::pixelToCell(int pixelX, int pixelY, int& cellX, int& cellY) const
{
    cellX = pixelX / Cell::WIDTH;
    cellY = pixelY / Cell::HEIGHT;
}

void World::restoreLastDragCell()
{
    if (lastDragCellX >= 0 && lastDragCellY >= 0 && lastDragCellX < static_cast<int>(width)
        && lastDragCellY < static_cast<int>(height)) {
        Cell& cell = at(lastDragCellX, lastDragCellY);
        cell.update(lastCellOriginalDirt, cell.com, cell.v);
        cell.markDirty();
        // Don't modify velocity or COM when restoring - they'll be set properly in endDragging
    }
    lastDragCellX = -1;
    lastDragCellY = -1;
    lastCellOriginalDirt = 0.0;
}

void World::startDragging(int pixelX, int pixelY)
{
    // Mark user input for time reversal
    markUserInput();

    int cellX, cellY;
    pixelToCell(pixelX, pixelY, cellX, cellY);

    // Check if coordinates are within bounds
    if (cellX >= 0 && cellX < static_cast<int>(width) && cellY >= 0
        && cellY < static_cast<int>(height)) {
        Cell& cell = at(cellX, cellY);

        // Only start dragging if there's dirt to drag
        if (cell.dirt > MIN_DIRT_THRESHOLD) {
            isDragging = true;
            dragStartX = cellX;
            dragStartY = cellY;
            draggedDirt = cell.dirt;
            draggedVelocity = cell.v;

            // Calculate initial COM based on cursor position within cell
            double subCellX = (pixelX % Cell::WIDTH) / static_cast<double>(Cell::WIDTH);
            double subCellY = (pixelY % Cell::HEIGHT) / static_cast<double>(Cell::HEIGHT);
            // Map from [0,1] to [-1,1]
            draggedCom = Vector2d(subCellX * 2.0 - 1.0, subCellY * 2.0 - 1.0);

            // Clear the source cell
            cell.update(0.0, Vector2d(0.0, 0.0), Vector2d(0.0, 0.0));
            cell.markDirty();

            // Visual feedback: fill the cell under the cursor
            restoreLastDragCell();
            lastDragCellX = cellX;
            lastDragCellY = cellY;
            lastCellOriginalDirt = 0.0; // Already set to 0 above
            cell.update(draggedDirt, draggedCom, cell.v);
            cell.markDirty();

            // Initialize recent positions
            recentPositions.clear();
            recentPositions.push_back({ cellX, cellY });
        }
    }
}

void World::updateDrag(int pixelX, int pixelY)
{
    if (!isDragging) return;

    int cellX, cellY;
    pixelToCell(pixelX, pixelY, cellX, cellY);

    if (cellX >= 0 && cellX < static_cast<int>(width) && cellY >= 0
        && cellY < static_cast<int>(height)) {
        // Calculate velocity based on drag movement
        double dx = cellX - dragStartX;
        double dy = cellY - dragStartY;
        draggedVelocity = Vector2d(dx * 2.0, dy * 2.0); // Scale factor for better feel

        // Calculate COM based on cursor position within cell
        double subCellX = (pixelX % Cell::WIDTH) / static_cast<double>(Cell::WIDTH);
        double subCellY = (pixelY % Cell::HEIGHT) / static_cast<double>(Cell::HEIGHT);
        // Map from [0,1] to [-1,1]
        draggedCom = Vector2d(subCellX * 2.0 - 1.0, subCellY * 2.0 - 1.0);

        // Visual feedback: restore previous cell, fill new cell
        if (cellX != lastDragCellX || cellY != lastDragCellY) {
            restoreLastDragCell();
            Cell& cell = at(cellX, cellY);
            lastDragCellX = cellX;
            lastDragCellY = cellY;
            lastCellOriginalDirt = cell.dirt;
            cell.update(draggedDirt, draggedCom, cell.v);
            cell.markDirty();
        }
        else {
            // Update COM even if we're in the same cell
            Cell& cell = at(cellX, cellY);
            cell.update(cell.dirt, draggedCom, cell.v);
            cell.markDirty();
        }

        // Track recent positions
        recentPositions.push_back({ cellX, cellY });
        if (recentPositions.size() > MAX_RECENT_POSITIONS) {
            recentPositions.erase(recentPositions.begin());
        }

        // Debug: Print current drag state
        LOG_DEBUG(
            "Drag Update - Cell: ("
            << cellX << "," << cellY << ") COM: (" << draggedCom.x << "," << draggedCom.y
            << ") Recent positions: " << recentPositions.size() << " Current velocity: ("
            << draggedVelocity.x << "," << draggedVelocity.y << ")");
    }
}

void World::endDragging(int pixelX, int pixelY)
{
    if (!isDragging) return;

    // Mark user input for time reversal
    markUserInput();

    int cellX, cellY;
    pixelToCell(pixelX, pixelY, cellX, cellY);

    // Debug: Print recent positions before calculating velocity
    LOG_DEBUG("Release Debug:");
    LOG_DEBUG("Recent positions: ");
    for (const auto& pos : recentPositions) {
        LOG_DEBUG("(" << pos.first << "," << pos.second << ") ");
    }
    LOG_DEBUG("");

    // Calculate final COM based on cursor position within cell
    double subCellX = (pixelX % Cell::WIDTH) / static_cast<double>(Cell::WIDTH);
    double subCellY = (pixelY % Cell::HEIGHT) / static_cast<double>(Cell::HEIGHT);
    // Map from [0,1] to [-1,1]
    draggedCom = Vector2d(subCellX * 2.0 - 1.0, subCellY * 2.0 - 1.0);
    LOG_DEBUG("Final COM before placement: (" << draggedCom.x << "," << draggedCom.y << ")");

    // Restore the last cell before finalizing
    restoreLastDragCell();

    // Check if coordinates are within bounds
    if (cellX >= 0 && cellX < static_cast<int>(width) && cellY >= 0
        && cellY < static_cast<int>(height)) {
        // Calculate average velocity from recent positions
        if (recentPositions.size() > 1) {
            double avgDx = 0.0, avgDy = 0.0;
            for (size_t i = 1; i < recentPositions.size(); ++i) {
                double dx = recentPositions[i].first - recentPositions[i - 1].first;
                double dy = recentPositions[i].second - recentPositions[i - 1].second;
                avgDx += dx;
                avgDy += dy;
                LOG_DEBUG("Step " << i << " delta: (" << dx << "," << dy << ")");
            }
            avgDx /= (recentPositions.size() - 1);
            avgDy /= (recentPositions.size() - 1);
            // Scale by cell size and a factor for better feel
            draggedVelocity = Vector2d(avgDx * Cell::WIDTH * 2.0, avgDy * Cell::HEIGHT * 2.0);

            LOG_DEBUG(
                "Final velocity before placement: (" << draggedVelocity.x << ","
                                                     << draggedVelocity.y << ")");
        }
        else {
            LOG_DEBUG("Not enough positions for velocity calculation");
        }

        // Queue up the drag end state for processing in advanceTime
        pendingDragEnd.hasPendingEnd = true;
        pendingDragEnd.cellX = cellX;
        pendingDragEnd.cellY = cellY;
        pendingDragEnd.dirt = draggedDirt;
        pendingDragEnd.velocity = draggedVelocity;
        pendingDragEnd.com = draggedCom;

        LOG_DEBUG(
            "Queued drag end at (" << cellX << "," << cellY << ") with velocity ("
                                   << draggedVelocity.x << "," << draggedVelocity.y << ") and COM ("
                                   << draggedCom.x << "," << draggedCom.y << ")");
    }

    // Reset drag state
    isDragging = false;
    draggedDirt = 0.0;
    draggedVelocity = Vector2d(0.0, 0.0);
    draggedCom = Vector2d(0.0, 0.0);
    recentPositions.clear();
}

void World::updateCursorForce(int pixelX, int pixelY, bool isActive)
{
    if (!cursorForceEnabled) return;

    cursorForceActive = isActive;
    if (isActive) {
        pixelToCell(pixelX, pixelY, cursorForceX, cursorForceY);
    }
}

// ConfigurableWorldSetup control methods
void World::setLeftThrowEnabled(bool enabled)
{
    ConfigurableWorldSetup* configSetup = dynamic_cast<ConfigurableWorldSetup*>(worldSetup.get());
    if (configSetup) {
        configSetup->setLeftThrowEnabled(enabled);
    }
}

void World::setRightThrowEnabled(bool enabled)
{
    ConfigurableWorldSetup* configSetup = dynamic_cast<ConfigurableWorldSetup*>(worldSetup.get());
    if (configSetup) {
        configSetup->setRightThrowEnabled(enabled);
    }
}

void World::setLowerRightQuadrantEnabled(bool enabled)
{
    ConfigurableWorldSetup* configSetup = dynamic_cast<ConfigurableWorldSetup*>(worldSetup.get());
    if (configSetup) {
        configSetup->setLowerRightQuadrantEnabled(enabled);
    }
}

void World::setWallsEnabled(bool enabled)
{
    ConfigurableWorldSetup* configSetup = dynamic_cast<ConfigurableWorldSetup*>(worldSetup.get());
    if (configSetup) {
        configSetup->setWallsEnabled(enabled);
    }
}

void World::setRainRate(double rate)
{
    ConfigurableWorldSetup* configSetup = dynamic_cast<ConfigurableWorldSetup*>(worldSetup.get());
    if (configSetup) {
        configSetup->setRainRate(rate);
    }
}

void World::resizeGrid(uint32_t newWidth, uint32_t newHeight, bool clearHistoryFlag)
{
    // Don't resize if dimensions haven't changed
    if (newWidth == width && newHeight == height) {
        return;
    }

    // Clear time reversal history only if requested (e.g., for structural changes vs cell size
    // changes)
    if (clearHistoryFlag) {
        clearHistory();
    }
    else {
        // Mark this as user input for time reversal when preserving history
        markUserInput();
    }

    // Capture current world state before resizing using WorldSetup
    uint32_t oldWidth = width;
    uint32_t oldHeight = height;
    std::vector<WorldSetup::ResizeData> oldState = worldSetup->captureWorldState(*this);

    // Clear existing cells and resize
    cells.clear();
    width = newWidth;
    height = newHeight;
    cells.resize(width * height);

    // Initialize new cells with default values
    for (auto& cell : cells) {
        cell.update(0.0, Vector2d(0.0, 0.0), Vector2d(0.0, 0.0));
        cell.water = 0.0;
    }

    // Apply the preserved state using feature-preserving interpolation
    if (!oldState.empty()) {
        worldSetup->applyWorldState(*this, oldState, oldWidth, oldHeight);
    }
    else {
        // If no previous state, use the default world setup
        reset();
    }
}

bool World::isLeftThrowEnabled() const
{
    const ConfigurableWorldSetup* configSetup =
        dynamic_cast<const ConfigurableWorldSetup*>(worldSetup.get());
    return configSetup ? configSetup->isLeftThrowEnabled() : false;
}

bool World::isRightThrowEnabled() const
{
    const ConfigurableWorldSetup* configSetup =
        dynamic_cast<const ConfigurableWorldSetup*>(worldSetup.get());
    return configSetup ? configSetup->isRightThrowEnabled() : false;
}

bool World::isLowerRightQuadrantEnabled() const
{
    const ConfigurableWorldSetup* configSetup =
        dynamic_cast<const ConfigurableWorldSetup*>(worldSetup.get());
    return configSetup ? configSetup->isLowerRightQuadrantEnabled() : false;
}

bool World::areWallsEnabled() const
{
    const ConfigurableWorldSetup* configSetup =
        dynamic_cast<const ConfigurableWorldSetup*>(worldSetup.get());
    return configSetup ? configSetup->areWallsEnabled() : false;
}

double World::getRainRate() const
{
    const ConfigurableWorldSetup* configSetup =
        dynamic_cast<const ConfigurableWorldSetup*>(worldSetup.get());
    return configSetup ? configSetup->getRainRate() : 0.0;
}

// Time reversal implementation
void World::saveWorldState()
{
    if (!timeReversalEnabled) return;

    // If we're not at the most recent position, truncate history from current position
    if (currentHistoryIndex >= 0) {
        stateHistory.erase(stateHistory.begin() + currentHistoryIndex + 1, stateHistory.end());
        currentHistoryIndex = -1;
    }

    // Create a snapshot of the current world state
    WorldState state;
    state.cells = cells;             // Deep copy of all cells
    state.width = width;             // Capture current grid width
    state.height = height;           // Capture current grid height
    state.cellWidth = Cell::WIDTH;   // Capture current cell width
    state.cellHeight = Cell::HEIGHT; // Capture current cell height
    state.timestep = timestep;
    state.totalMass = totalMass;
    state.removedMass = removedMass;
    state.timestamp = simulationTime; // Capture current simulation time

    stateHistory.push_back(std::move(state));

    // Limit history size to prevent memory issues
    if (stateHistory.size() > MAX_HISTORY_SIZE) {
        stateHistory.erase(stateHistory.begin());
    }

    LOG_DEBUG(
        "Saved world state at time " << simulationTime
                                     << "s. History size: " << stateHistory.size());
}

void World::goBackward()
{
    if (!canGoBackward()) return;

    // If we're at the current state (not navigating history yet), capture current state
    if (currentHistoryIndex == -1 && !hasStoredCurrentState) {
        currentLiveState.cells = cells;
        currentLiveState.width = width;
        currentLiveState.height = height;
        currentLiveState.cellWidth = Cell::WIDTH;
        currentLiveState.cellHeight = Cell::HEIGHT;
        currentLiveState.timestep = timestep;
        currentLiveState.totalMass = totalMass;
        currentLiveState.removedMass = removedMass;
        currentLiveState.timestamp = simulationTime;
        hasStoredCurrentState = true;
        LOG_DEBUG("Captured current live state for time reversal navigation");
    }

    if (currentHistoryIndex == -1) {
        // Moving back from current state - go to last saved state
        currentHistoryIndex = static_cast<int>(stateHistory.size()) - 1;
    }
    else if (currentHistoryIndex > 0) {
        // Move further back in history
        currentHistoryIndex--;
    }
    else {
        // Already at the earliest state
        return;
    }

    // Restore the state using the helper method that handles grid size changes
    const WorldState& state = stateHistory[currentHistoryIndex];
    restoreWorldState(state);

    LOG_DEBUG(
        "Restored world state from history index: " << currentHistoryIndex
                                                    << " (timestep: " << timestep
                                                    << ", time: " << simulationTime << "s)");
}

void World::goForward()
{
    if (!canGoForward()) return;

    currentHistoryIndex++;

    // If we've reached beyond the saved history, restore to current live state
    if (currentHistoryIndex >= static_cast<int>(stateHistory.size())) {
        currentHistoryIndex = -1; // Reset to current state indicator

        // Restore the captured current live state if we have it
        if (hasStoredCurrentState) {
            restoreWorldState(currentLiveState);
            hasStoredCurrentState = false; // Clear the stored state
            LOG_DEBUG("Restored captured current live state");
        }
        return;
    }

    // Restore the state using the helper method that handles grid size changes
    const WorldState& state = stateHistory[currentHistoryIndex];
    restoreWorldState(state);

    LOG_DEBUG(
        "Restored world state from history index: " << currentHistoryIndex
                                                    << " (timestep: " << timestep
                                                    << ", time: " << simulationTime << "s)");
}

void World::restoreWorldState(const WorldState& state)
{
    // Restore cell size first (affects grid calculations)
    if (Cell::WIDTH != state.cellWidth || Cell::HEIGHT != state.cellHeight) {
        LOG_DEBUG(
            "Cell size changed from " << Cell::WIDTH << "x" << Cell::HEIGHT << " to "
                                      << state.cellWidth << "x" << state.cellHeight
                                      << " during restore");
    }
    Cell::WIDTH = state.cellWidth;
    Cell::HEIGHT = state.cellHeight;

    // Check if grid size has changed
    if (state.width != width || state.height != height) {
        LOG_DEBUG(
            "Grid size changed from " << width << "x" << height << " to " << state.width << "x"
                                      << state.height << " during restore");

        // Resize the grid to match the stored state
        cells.clear();
        width = state.width;
        height = state.height;
        cells.resize(width * height);

        // Initialize new cells with default values
        for (auto& cell : cells) {
            cell.update(0.0, Vector2d(0.0, 0.0), Vector2d(0.0, 0.0));
            cell.water = 0.0;
        }
    }

    // Now restore the cells (sizes are guaranteed to match)
    cells = state.cells;
    timestep = state.timestep;
    totalMass = state.totalMass;
    removedMass = state.removedMass;
    simulationTime = state.timestamp;

    // Mark all cells as needing redraw
    for (auto& cell : cells) {
        cell.markDirty();
    }
}
