#include "World.h"
#include "Cell.h"
#include "SimulatorUI.h"
#include "Timers.h"
#include "WorldSetup.h"
#include "Vector2i.h"
#include "WorldInterpolationTool.h"
#include "spdlog/spdlog.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>
#include <random>
#include <sstream>
#include <stdexcept>

#ifndef M_PI
#define M_PI 3.14159265358979323846
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
    : draw_area(draw_area), width(width), height(height), cells(width * height), selected_material_(MaterialType::DIRT), ui_ref_(nullptr)
{
    spdlog::info("Creating World: {}x{} grid ({} total cells)", width, height, cells.size());

    // Initialize timers
    timers.startTimer("total_simulation");

    // Create configurable world setup
    worldSetup = std::make_unique<ConfigurableWorldSetup>();

}

World::~World()
{
    spdlog::info("Destroying World: {}x{} grid", width, height);

    // Stop the total simulation timer and dump stats
    timers.stopTimer("total_simulation");
    timers.dumpTimerStats();
}

void World::advanceTime(double deltaTimeSeconds)
{
    timers.startTimer("advance_time");

    spdlog::trace(
        "advanceTime: deltaTime={:.4f}s, simulationTime={:.3f}s", deltaTimeSeconds, simulationTime);

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

    // Use the physics rules to update pressures and apply forces
    spdlog::trace("Running physics rules: RulesA");
    updateAllPressures(deltaTimeSeconds);
    applyPressure(deltaTimeSeconds);

    processTransfers(deltaTimeSeconds);

    applyMoves(); // Apply the pressure moves

    // Update UI with the new total mass, calculated on the fly.
    if (ui_) {
        ui_->updateMassLabel(getTotalMass());
    }

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

            spdlog::info(
                "Processed drag end at ({},{}) with velocity ({:.2f},{:.2f}) and COM "
                "({:.2f},{:.2f})",
                pendingDragEnd.cellX,
                pendingDragEnd.cellY,
                cell.v.x,
                cell.v.y,
                cell.com.x,
                cell.com.y);
        }
        pendingDragEnd.hasPendingEnd = false;
        timers.stopTimer("process_drag_end");
    }
}

void World::processTransfers(double deltaTimeSeconds)
{
    spdlog::trace("processTransfers: processing {}x{} cells", width, height);

    // Collect potential moves
    for (uint32_t y = 0; y < height; y++) {
        for (uint32_t x = 0; x < width; x++) {
            // Skip the cell being dragged
            if (isDragging && static_cast<int>(x) == lastDragCellX
                && static_cast<int>(y) == lastDragCellY) {
                continue;
            }
            Cell& cell = at(x, y);

            // Skip truly empty cells only (removed aggressive mass removal)
            if (cell.percentFull() <= 0.0) {
                continue;
            }

            // Apply physics using the current rules
            applyPhysicsToCell(cell, x, y, deltaTimeSeconds);

            // Calculate predicted COM position after this timestep
            Vector2d predictedCom = cell.com + cell.v * deltaTimeSeconds;

            // CRITICAL: Bound the predicted COM to prevent unbounded growth
            // COM should stay within reasonable limits even with high velocities
            const double MAX_COM_BOUND =
                2.0; // Allow some overshoot beyond threshold for proper transfer detection
            predictedCom.x = std::max(-MAX_COM_BOUND, std::min(MAX_COM_BOUND, predictedCom.x));
            predictedCom.y = std::max(-MAX_COM_BOUND, std::min(MAX_COM_BOUND, predictedCom.y));

            // Check for transfers based on COM deflection
            bool shouldTransferX, shouldTransferY;
            int targetX, targetY;
            Vector2d comOffset;
            calculateTransferDirection(
                cell,
                shouldTransferX,
                shouldTransferY,
                targetX,
                targetY,
                comOffset,
                x,
                y);

            bool transferOccurred = false;
            bool attemptedTransfer = shouldTransferX || shouldTransferY;

            if (attemptedTransfer) {
                const double totalMass = cell.dirt + cell.water;
                bool isTargetInBounds = isWithinBounds(targetX, targetY);

                // Try diagonal transfer first if both X and Y are needed and target is valid
                if (shouldTransferX && shouldTransferY && isTargetInBounds) {
                    transferOccurred =
                        attemptTransfer(cell, x, y, targetX, targetY, comOffset, totalMass);
                }

                // If no diagonal transfer, or it wasn't attempted, try cardinal directions
                if (!transferOccurred) {
                    // GRAVITY PRIORITY: For dirt particles, prioritize downward movement over
                    // horizontal This prevents dirt from getting "stuck" horizontally when it
                    // should be falling
                    bool isDirtCell = cell.dirt > MIN_MATTER_THRESHOLD;
                    bool isDownwardTransfer = shouldTransferY && targetY > static_cast<int>(y);

                    if (isDirtCell && isDownwardTransfer) {
                        // Prioritize downward transfer for dirt (gravity wins)
                        if (isWithinBounds(x, targetY)) {
                            Vector2d yComOffset = comOffset;
                            yComOffset.x = cell.com.x; // Keep original X component
                            transferOccurred =
                                attemptTransfer(cell, x, y, x, targetY, yComOffset, totalMass);
                        }
                        // Try horizontal only if downward failed
                        if (!transferOccurred && shouldTransferX && isWithinBounds(targetX, y)) {
                            Vector2d xComOffset = comOffset;
                            xComOffset.y = cell.com.y; // Keep original Y component
                            transferOccurred =
                                attemptTransfer(cell, x, y, targetX, y, xComOffset, totalMass);
                        }
                    }
                    else {
                        // For water or upward movement, use original priority (horizontal first)
                        if (shouldTransferX && isWithinBounds(targetX, y)) {
                            Vector2d xComOffset = comOffset;
                            xComOffset.y = cell.com.y; // Keep original Y component
                            transferOccurred =
                                attemptTransfer(cell, x, y, targetX, y, xComOffset, totalMass);
                        }
                        // Try Y transfer if X transfer didn't occur or wasn't needed
                        if (!transferOccurred && shouldTransferY && isWithinBounds(x, targetY)) {
                            Vector2d yComOffset = comOffset;
                            yComOffset.x = cell.com.x; // Keep original X component
                            transferOccurred =
                                attemptTransfer(cell, x, y, x, targetY, yComOffset, totalMass);
                        }
                    }
                }

                // If a transfer was attempted but failed, handle the collision/blockage
                if (!transferOccurred) {
                    handleTransferFailure(
                        cell, x, y, targetX, targetY, shouldTransferX, shouldTransferY);
                }
            }

            // If no transfer was attempted, update COM internally
            if (!attemptedTransfer) {
                cell.update(cell.dirt, predictedCom, cell.v);
                checkExcessiveDeflectionReflection(cell);
            }
        }
    }
}

void World::applyPhysicsToCell(Cell& cell, uint32_t x, uint32_t y, double deltaTimeSeconds)
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
    cell.v.y += gravity * deltaTimeSeconds;

    // Apply water physics (cohesion, viscosity) and buoyancy
    for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            if (dx == 0 && dy == 0) continue;
            
            Vector2i offset(dx, dy);
            Vector2i neighborPos(x, y);
            neighborPos += offset;
            
            if (isWithinBounds(neighborPos.x, neighborPos.y)) {
                Cell& neighbor = at(neighborPos.x, neighborPos.y);

                // Apply water cohesion and viscosity if this is a water cell
                if (cell.water >= MIN_MATTER_THRESHOLD) {
                    Vector2d cohesion = cell.calculateWaterCohesion(cell, neighbor, this, x, y);
                    cell.v += cohesion * deltaTimeSeconds;
                    cell.applyViscosity(neighbor);
                }

                // Apply buoyancy forces (works on any cell with dirt or water)
                Vector2d buoyancy = cell.calculateBuoyancy(cell, neighbor, offset);
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
        spdlog::trace("  Transfer blocked by full cell at ({},{})", targetX, targetY);
        return false;
    }

    // Calculate transfer amounts
    const double availableSpace = 1.0 - targetCell.percentFull();
    const double safeAvailableSpace =
        std::max(0.0, availableSpace - 0.01); // Leave 1% safety margin
    const double moveAmount = std::min(totalMass, safeAvailableSpace * TRANSFER_FACTOR);

    // Don't transfer if move amount is zero
    if (moveAmount <= 0.0) {
        return false;
    }

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

void World::handleTransferFailure(
    Cell& cell,
    uint32_t x,
    uint32_t y,
    int targetX,
    int targetY,
    bool shouldTransferX,
    bool shouldTransferY)
{
    // Comprehensive transfer failure handling that conserves momentum

    // First, handle boundary collisions (out of bounds)
    if (shouldTransferX && !isWithinBounds(targetX, y)) {
        // Horizontal boundary collision
        cell.v.x = -cell.v.x * ELASTICITY_FACTOR;
        cell.com.x =
            (targetX < 0) ? -Cell::COM_DEFLECTION_THRESHOLD : Cell::COM_DEFLECTION_THRESHOLD;
        spdlog::trace("  X boundary reflection: COM.x={}, v.x={}", cell.com.x, cell.v.x);
    }

    if (shouldTransferY && !isWithinBounds(x, targetY)) {
        // Vertical boundary collision
        cell.v.y = -cell.v.y * ELASTICITY_FACTOR;
        cell.com.y =
            (targetY < 0) ? -Cell::COM_DEFLECTION_THRESHOLD : Cell::COM_DEFLECTION_THRESHOLD;
        spdlog::trace("  Y boundary reflection: COM.y={}, v.y={}", cell.com.y, cell.v.y);
    }

    // Handle in-bounds collisions (blocked by other cells)
    bool hitHorizontalObstacle = false;
    bool hitVerticalObstacle = false;

    if (shouldTransferX && isWithinBounds(targetX, y)) {
        double targetFullness = at(targetX, y).percentFull();
        if (targetFullness >= 0.95) { // Nearly full cell blocks transfer
            hitHorizontalObstacle = true;
            cell.v.x = -cell.v.x * ELASTICITY_FACTOR;
            cell.com.x =
                (cell.com.x > 0) ? Cell::COM_DEFLECTION_THRESHOLD : -Cell::COM_DEFLECTION_THRESHOLD;
            spdlog::trace(
                "  X collision with full cell ({},{}): fullness={}", targetX, y, targetFullness);
        }
        else if (targetFullness > 0.7) {
            // Partial blockage - reduce momentum without full reflection
            double blockageFactor = (targetFullness - 0.7) / 0.25; // 0.0 to 1.0
            cell.v.x *= (1.0 - blockageFactor * 0.5);              // Reduce by up to 50%
            cell.com.x *= (1.0 - blockageFactor * 0.3);            // Reduce COM deflection
            spdlog::trace("  X partial blockage: factor={}, new v.x={}", blockageFactor, cell.v.x);
        }
    }

    if (shouldTransferY && isWithinBounds(x, targetY)) {
        double targetFullness = at(x, targetY).percentFull();
        if (targetFullness >= 0.95) { // Nearly full cell blocks transfer
            hitVerticalObstacle = true;
            cell.v.y = -cell.v.y * ELASTICITY_FACTOR;
            cell.com.y =
                (cell.com.y > 0) ? Cell::COM_DEFLECTION_THRESHOLD : -Cell::COM_DEFLECTION_THRESHOLD;
            spdlog::trace(
                "  Y collision with full cell ({},{}): fullness={}", x, targetY, targetFullness);
        }
        else if (targetFullness > 0.7) {
            // Partial blockage - reduce momentum without full reflection
            double blockageFactor = (targetFullness - 0.7) / 0.25; // 0.0 to 1.0
            cell.v.y *= (1.0 - blockageFactor * 0.5);              // Reduce by up to 50%
            cell.com.y *= (1.0 - blockageFactor * 0.3);            // Reduce COM deflection
            spdlog::trace("  Y partial blockage: factor={}, new v.y={}", blockageFactor, cell.v.y);
        }
    }

    // Handle diagonal collisions (corner case)
    if (shouldTransferX && shouldTransferY && isWithinBounds(targetX, targetY)) {
        double diagonalFullness = at(targetX, targetY).percentFull();
        if (diagonalFullness >= 0.95) {
            // Diagonal collision - reflect both components
            if (!hitHorizontalObstacle) {
                cell.v.x = -cell.v.x * ELASTICITY_FACTOR;
                cell.com.x = (cell.com.x > 0) ? Cell::COM_DEFLECTION_THRESHOLD
                                              : -Cell::COM_DEFLECTION_THRESHOLD;
            }
            if (!hitVerticalObstacle) {
                cell.v.y = -cell.v.y * ELASTICITY_FACTOR;
                cell.com.y = (cell.com.y > 0) ? Cell::COM_DEFLECTION_THRESHOLD
                                              : -Cell::COM_DEFLECTION_THRESHOLD;
            }
            spdlog::trace("  Diagonal collision with full cell ({},{})", targetX, targetY);
        }
    }

    // Monte Carlo transfer failure - momentum bleeding to prevent buildup
    // This handles the case where transfer was blocked not due to physics, but due to Monte Carlo
    // selection
    if (!hitHorizontalObstacle && !hitVerticalObstacle && (shouldTransferX || shouldTransferY)) {
        // Apply gentle momentum damping to prevent infinite acceleration
        const double MOMENTUM_BLEED_FACTOR = 0.02; // 2% velocity reduction per failed transfer

        if (shouldTransferX && isWithinBounds(targetX, y) && at(targetX, y).percentFull() < 0.7) {
            // Transfer could have succeeded but was blocked by Monte Carlo - apply momentum bleed
            cell.v.x *= (1.0 - MOMENTUM_BLEED_FACTOR);
            cell.com.x *= (1.0 - MOMENTUM_BLEED_FACTOR * 0.5);
            spdlog::trace("  Monte Carlo X momentum bleed: new v.x={}", cell.v.x);
        }

        if (shouldTransferY && isWithinBounds(x, targetY) && at(x, targetY).percentFull() < 0.7) {
            // Transfer could have succeeded but was blocked by Monte Carlo - apply momentum bleed
            cell.v.y *= (1.0 - MOMENTUM_BLEED_FACTOR);
            cell.com.y *= (1.0 - MOMENTUM_BLEED_FACTOR * 0.5);
            spdlog::trace("  Monte Carlo Y momentum bleed: new v.y={}", cell.v.y);
        }
    }
}

void World::handleBoundaryReflection(
    Cell& /* cell */, int /* targetX */, int /* targetY */, bool /* shouldTransferX */, bool /* shouldTransferY */)
{
    // Legacy method - now handled by handleTransferFailure
    // Kept for compatibility but functionality moved to handleTransferFailure
}

void World::checkExcessiveDeflectionReflection(Cell& cell)
{
    Vector2d normalizedDeflection = cell.getNormalizedDeflection();

    if (std::abs(normalizedDeflection.x) > REFLECTION_THRESHOLD) {
        cell.v.x = -cell.v.x * ELASTICITY_FACTOR;
        cell.com.x = (normalizedDeflection.x > 0) ? Cell::COM_DEFLECTION_THRESHOLD
                                                  : -Cell::COM_DEFLECTION_THRESHOLD;
        spdlog::trace("  Horizontal reflection: COM.x={}, v.x={}", cell.com.x, cell.v.x);
    }

    if (std::abs(normalizedDeflection.y) > REFLECTION_THRESHOLD) {
        cell.v.y = -cell.v.y * ELASTICITY_FACTOR;
        cell.com.y = (normalizedDeflection.y > 0) ? Cell::COM_DEFLECTION_THRESHOLD
                                                  : -Cell::COM_DEFLECTION_THRESHOLD;
        spdlog::trace("  Vertical reflection: COM.y={}, v.y={}", cell.com.y, cell.v.y);
    }
}

bool World::isWithinBounds(int x, int y) const
{
    return x >= 0 && x < static_cast<int>(width) && y >= 0 && y < static_cast<int>(height);
}

bool World::isWithinBounds(const Vector2i& pos) const
{
    return isWithinBounds(pos.x, pos.y);
}

Vector2d World::calculateNaturalCOM(const Vector2d& sourceCOM, int deltaX, int deltaY)
{
    // This function calculates the "natural" position of the COM in the target cell
    // if it were to continue its trajectory uninterrupted.
    // The old logic was a simple subtraction, which was incorrect.
    // The new logic correctly "wraps" the COM around to the other side of the cell.

    Vector2d naturalCOM = sourceCOM;

    // If moving right (deltaX = 1), COM enters from the left side.
    if (deltaX > 0) {
        naturalCOM.x -= COM_CELL_WIDTH;
    }
    // If moving left (deltaX = -1), COM enters from the right side.
    else if (deltaX < 0) {
        naturalCOM.x += COM_CELL_WIDTH;
    }

    // If moving down (deltaY = 1), COM enters from the top side.
    if (deltaY > 0) {
        naturalCOM.y -= COM_CELL_WIDTH;
    }
    // If moving up (deltaY = -1), COM enters from the bottom side.
    else if (deltaY < 0) {
        naturalCOM.y += COM_CELL_WIDTH;
    }

    return naturalCOM;
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

double World::getTotalMass() const
{
    double currentTotalMass = 0.0;
    for (uint32_t y = 0; y < height; y++) {
        for (uint32_t x = 0; x < width; x++) {
            currentTotalMass += at(x, y).percentFull();
        }
    }
    return currentTotalMass;
}

void World::setUI(std::unique_ptr<SimulatorUI> ui)
{
    ui_ = std::move(ui);
}

void World::setUIReference(SimulatorUI* ui)
{
    ui_ref_ = ui;
    spdlog::debug("UI reference set for World");
}


void World::applyPressure(const double /* deltaTimeSeconds */)
{
    // Random number generator for probabilistic pressure application
    static std::mt19937 rng(std::random_device{}());
    static std::uniform_real_distribution<double> dist(0.0, 1.0);

    int pressureApplications = 0; // Debug counter

    for (uint32_t y = 0; y < height; y++) {
        for (uint32_t x = 0; x < width; x++) {
            Cell& cell = at(x, y);

            // Skip empty cells or cells with negligible pressure
            if (cell.percentFull() < MIN_MATTER_THRESHOLD) continue;

            if (cell.pressure.mag() < 0.001) continue;

            // Calculate pressure thresholds based on material type
            double pressureThreshold;
            double maxPressureForFullProb;
            if (cell.water > cell.dirt) {
                // Water flows easily - very low threshold
                pressureThreshold = waterPressureThreshold;
                maxPressureForFullProb = pressureThreshold * 10.0;
            }
            else {
                // Dirt is more solid
                pressureThreshold = 0.005;
                maxPressureForFullProb = pressureThreshold * 20.0;
            }

            double pressureMagnitude = cell.pressure.mag();
            ASSERT(std::isfinite(pressureMagnitude), "Pressure magnitude is NaN or infinite");

            if (pressureMagnitude < pressureThreshold) continue;

            double excessPressure = pressureMagnitude - pressureThreshold;
            double maxExcess = maxPressureForFullProb - pressureThreshold;
            double probability = std::min(1.0, excessPressure / maxExcess);

            if (dist(rng) > probability) continue;

            Vector2d desiredDirection = cell.pressure.normalize();
            double pressureForce = pressureMagnitude * pressureScale;

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

            bool canApplyDirectly = false;
            if (isWithinBounds(targetX, targetY)) {
                if (at(targetX, targetY).percentFull() < 0.95) {
                    canApplyDirectly = true;
                }
            }
            else {
                canApplyDirectly = true; // Pressure against boundary
            }

            Vector2d appliedDirection;
            double forceMultiplier = 1.0;
            bool fragmented = false;
            bool isWaterCell = (cell.water > cell.dirt);

            if (canApplyDirectly) {
                appliedDirection = desiredDirection;
            }
            else {
                bool foundOpenNeighbor = false;
                if (isWaterCell) {
                    // WATER-SPECIFIC LOGIC
                    Vector2d gravityDirection(0.0, 1.0);
                    Vector2d currentVelocity = cell.v.normalize();
                    double bestScore = -2.0;

                    for (int dy = -1; dy <= 1; dy++) {
                        for (int dx = -1; dx <= 1; dx++) {
                            if (dx == 0 && dy == 0) continue;
                            
                            Vector2i offset(dx, dy);
                            Vector2i neighborPos(static_cast<int>(x), static_cast<int>(y));
                            neighborPos += offset;

                            if (isWithinBounds(neighborPos.x, neighborPos.y)) {
                                Cell& neighbor = at(neighborPos.x, neighborPos.y);
                                if (neighbor.percentFull() < 0.95) {
                                    Vector2d neighborDirection = Vector2d(offset.x, offset.y).normalize();
                                    if (neighborDirection.dot(gravityDirection) >= -0.1) {
                                        double velocityScore = (cell.v.mag() > 0.1)
                                            ? neighborDirection.dot(currentVelocity) * 0.4
                                            : 0.0;
                                        double pressureScore =
                                            neighborDirection.dot(desiredDirection) * 0.3;
                                        double gravityScore =
                                            neighborDirection.dot(gravityDirection) * 0.3;
                                        double totalScore =
                                            velocityScore + pressureScore + gravityScore;

                                        if (totalScore > bestScore
                                            || (totalScore == bestScore && dist(rng) > 0.5)) {
                                            bestScore = totalScore;
                                            appliedDirection = neighborDirection;
                                            foundOpenNeighbor = true;
                                        }
                                    }
                                }
                                else {
                                    ASSERT(
                                        neighbor.percentFull() <= 1.10,
                                        "Neighbor cell is overfull before pressure application.");
                                }
                            }
                        }
                    }
                    if (!foundOpenNeighbor) continue;

                    forceMultiplier = 1.0;
                    double deflectionAngle = std::acos(
                        std::max(-1.0, std::min(1.0, desiredDirection.dot(appliedDirection))));
                    const double FRAGMENTATION_THRESHOLD = M_PI / 4.0;

                    if (deflectionAngle > FRAGMENTATION_THRESHOLD && cell.water > 0.1) {
                        Vector2d sidewaysVelocity =
                            cell.v - gravityDirection * cell.v.dot(gravityDirection);
                        double sidewaysSpeed = sidewaysVelocity.mag();
                        const double MAX_FRAGMENTATION = 0.4, MIN_FRAGMENTATION = 0.0,
                                     SPEED_THRESHOLD = 4.0;
                        double speedFactor = std::min(1.0, sidewaysSpeed / SPEED_THRESHOLD);
                        double fragmentationRatio = MAX_FRAGMENTATION * (1.0 - speedFactor)
                            + MIN_FRAGMENTATION * speedFactor;

                        int fragTargetX = static_cast<int>(x)
                            + (appliedDirection.x > 0.5 ? 1 : (appliedDirection.x < -0.5 ? -1 : 0));
                        int fragTargetY = static_cast<int>(y)
                            + (appliedDirection.y > 0.5 ? 1 : (appliedDirection.y < -0.5 ? -1 : 0));

                        if (isWithinBounds(fragTargetX, fragTargetY)) {
                            Cell& targetCell = at(fragTargetX, fragTargetY);
                            if (targetCell.percentFull() < 0.95) {
                                double availableSpace = 1.0 - targetCell.percentFull();
                                double fragmentAmount =
                                    std::min(cell.water * fragmentationRatio, availableSpace);

                                // IMPORTANT: Only fragment if the result pieces will be above
                                // threshold This prevents creating tiny water pieces that get
                                // removed later
                                if (fragmentAmount > MIN_MATTER_THRESHOLD * 2.0) {
                                    moves.push_back({ .fromX = x,
                                                      .fromY = y,
                                                      .toX = (uint32_t)fragTargetX,
                                                      .toY = (uint32_t)fragTargetY,
                                                      .dirtAmount = 0.0,
                                                      .waterAmount = fragmentAmount,
                                                      .comOffset = appliedDirection * 0.3 });
                                    // CRITICAL FIX: Don't decrement water here! Let applyMoves()
                                    // handle it properly This was causing mass loss because moves
                                    // were processed with already-decremented source cells
                                    // cell.water -= fragmentAmount;  // REMOVED
                                    fragmented = true;
                                }
                            }
                        }
                    }
                }
                else {
                    // DIRT LOGIC
                    double bestAngle = M_PI;
                    const double MAX_DEFLECTION_ANGLE = M_PI / 2.0;

                    for (int dy = -1; dy <= 1; dy++) {
                        for (int dx = -1; dx <= 1; dx++) {
                            if (dx == 0 && dy == 0) continue;
                            
                            Vector2i offset(dx, dy);
                            Vector2i neighborPos(static_cast<int>(x), static_cast<int>(y));
                            neighborPos += offset;

                            if (isWithinBounds(neighborPos.x, neighborPos.y)) {
                                if (at(neighborPos.x, neighborPos.y).percentFull() < 0.95) {
                                    Vector2d neighborDirection = Vector2d(offset.x, offset.y).normalize();
                                    double angle = std::acos(std::max(
                                        -1.0,
                                        std::min(1.0, desiredDirection.dot(neighborDirection))));
                                    if (angle <= MAX_DEFLECTION_ANGLE && angle < bestAngle) {
                                        bestAngle = angle;
                                        appliedDirection = neighborDirection;
                                        foundOpenNeighbor = true;
                                    }
                                }
                                else {
                                    // Skip pressure application to overfull cells to prevent
                                    // capacity violations
                                    if (at(neighborPos.x, neighborPos.y).percentFull() > 1.01) {
                                        spdlog::trace(
                                            "Skipping pressure application to overfull cell "
                                            "({},{}) with {}% capacity",
                                            neighborPos.x,
                                            neighborPos.y,
                                            at(neighborPos.x, neighborPos.y).percentFull() * 100);
                                        continue; // Skip this neighbor
                                    }
                                }
                            }
                        }
                    }
                    if (!foundOpenNeighbor) continue;
                    forceMultiplier = std::abs(std::cos(bestAngle));
                }
            }

            if (!fragmented) {
                // Use much gentler pressure forces to prevent aggressive water movement
                double forceScale = isWaterCell ? 5.0 : 20.0; // Water moves more gently
                double effectiveForce = pressureForce * forceMultiplier * forceScale;
                Vector2d pressureAcceleration = appliedDirection * effectiveForce;
                cell.v += pressureAcceleration;
                pressureApplications++;

                // Also use lower velocity caps for water to prevent wild movement
                const double MAX_PRESSURE_VELOCITY = isWaterCell ? 4.0 : 8.0;
                if (cell.v.mag() > MAX_PRESSURE_VELOCITY) {
                    cell.v = cell.v.normalize() * MAX_PRESSURE_VELOCITY;
                }
            }
        }
    }

    if (pressureApplications > 0) {
        spdlog::trace("Applied pressure to {} cells this frame", pressureApplications);
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

    spdlog::trace("=== PRESSURE GENERATION PHASE ===");
    int pressuresGenerated = 0;

    // Then calculate pressures that cells exert on their neighbor.
    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            Cell& cell = at(x, y);
            if (cell.percentFull() < MIN_MATTER_THRESHOLD) continue;
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
            if (x + 1 < width) {
                if (normalizedDeflection.x > 0.0) {
                    Cell& neighbor = at(x + 1, y);
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
                    Cell& neighbor = at(x - 1, y);
                    double pressureAdded = -normalizedDeflection.x * mass * deltaTimeSeconds;
                    neighbor.pressure.x += pressureAdded;
                    spdlog::trace(
                        "Adding pressure {} to left neighbor ({},{})", pressureAdded, (x - 1), y);
                    pressuresGenerated++;
                }
            }
            // Down neighbor
            if (y + 1 < height) {
                if (normalizedDeflection.y > 0.0) {
                    Cell& neighbor = at(x, y + 1);
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
                    Cell& neighbor = at(x, y - 1);
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
                        
                        Vector2i offset(dx, dy);
                        Vector2i neighborPos(x, y);
                        neighborPos += offset;
                        
                        if (neighborPos.x >= 0 && neighborPos.x < (int)width && 
                            neighborPos.y >= 0 && neighborPos.y < (int)height) {
                            sum += at(neighborPos.x, neighborPos.y).pressure;
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

void World::updateAllPressuresTopDown(double deltaTimeSeconds)
{
    // Clear all pressures first
    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            at(x, y).pressure = Vector2d(0.0, 0.0);
        }
    }

    spdlog::trace("=== TOP-DOWN PRESSURE GENERATION ===");

    // Process each column from top to bottom
    for (uint32_t x = 0; x < width; ++x) {
        double accumulatedMass = 0.0;
        Vector2d accumulatedPressure(0.0, 0.0);

        for (uint32_t y = 0; y < height; ++y) {
            Cell& cell = at(x, y);

            // Add this cell's mass to the accumulated total
            if (cell.percentFull() >= MIN_MATTER_THRESHOLD) {
                accumulatedMass += cell.percentFull();
            }

            // Calculate hydrostatic pressure from accumulated mass above
            // This increases linearly with depth and accumulated mass
            double hydrostaticPressure = accumulatedMass * gravity * deltaTimeSeconds * 0.1;

            // Apply base hydrostatic pressure downward
            cell.pressure.y += hydrostaticPressure;

            // Add lateral pressure based on COM deflection of all cells above
            // This allows pressure to "spread sideways" as it accumulates
            Vector2d lateralPressure(0.0, 0.0);

            // Look at cells above to determine lateral pressure direction
            for (uint32_t checkY = 0; checkY <= y; ++checkY) {
                Cell& upperCell = at(x, checkY);
                if (upperCell.percentFull() >= MIN_MATTER_THRESHOLD) {
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
    // This allows pressure to spread sideways between columns
    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            Cell& cell = at(x, y);
            if (cell.percentFull() < MIN_MATTER_THRESHOLD) continue;

            // Look at neighboring columns to determine pressure differences
            double leftPressure = (x > 0) ? at(x - 1, y).pressure.y : 0.0;
            double rightPressure = (x + 1 < width) ? at(x + 1, y).pressure.y : 0.0;
            double currentPressure = cell.pressure.y;

            // Calculate pressure gradients
            double leftGradient = currentPressure - leftPressure;
            double rightGradient = currentPressure - rightPressure;

            // Apply horizontal pressure based on gradients
            if (leftGradient > 0.001) {
                // Higher pressure than left neighbor - apply leftward pressure
                if (x > 0) {
                    at(x - 1, y).pressure.x += leftGradient * 0.1;
                }
            }
            if (rightGradient > 0.001) {
                // Higher pressure than right neighbor - apply rightward pressure
                if (x + 1 < width) {
                    at(x + 1, y).pressure.x += rightGradient * 0.1;
                }
            }
        }
    }
}

void World::updateAllPressuresIterativeSettling(double deltaTimeSeconds)
{
    // Clear all pressures first
    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            at(x, y).pressure = Vector2d(0.0, 0.0);
        }
    }

    spdlog::trace("=== ITERATIVE SETTLING PRESSURE GENERATION ===");

    // Multiple settling passes - each pass allows material to settle under pressure
    const int numSettlingPasses = 3;
    const double passTimeStep = deltaTimeSeconds / numSettlingPasses;

    for (int pass = 0; pass < numSettlingPasses; ++pass) {
        spdlog::trace("Settling pass {}/{}", (pass + 1), numSettlingPasses);

        // For each pass, work from top to bottom
        for (uint32_t y = 0; y < height; ++y) {
            for (uint32_t x = 0; x < width; ++x) {
                Cell& cell = at(x, y);
                if (cell.percentFull() < MIN_MATTER_THRESHOLD) continue;

                // Calculate pressure from mass above (looking at previous pass results)
                double pressureFromAbove = 0.0;
                for (uint32_t checkY = 0; checkY < y; ++checkY) {
                    Cell& upperCell = at(x, checkY);
                    if (upperCell.percentFull() >= MIN_MATTER_THRESHOLD) {
                        // Distance decay factor - closer cells contribute more pressure
                        double distanceFactor = 1.0 / (1.0 + (y - checkY) * 0.3);
                        pressureFromAbove += upperCell.percentFull() * gravity * distanceFactor;
                    }
                }

                // Apply settling pressure (increases with each pass)
                double settlingPressure = pressureFromAbove * passTimeStep * (pass + 1) * 0.02;
                cell.pressure.y += settlingPressure;

                // Add lateral pressure redistribution based on pressure differences
                Vector2d lateralPressure(0.0, 0.0);

                // Check neighboring columns for pressure differences
                if (x > 0) {
                    Cell& leftCell = at(x - 1, y);
                    double pressureDiff = cell.pressure.y - leftCell.pressure.y;
                    if (pressureDiff > 0.001) {
                        lateralPressure.x -= pressureDiff * 0.1; // Pressure flows left
                        leftCell.pressure.x += pressureDiff * 0.1;
                    }
                }

                if (x + 1 < width) {
                    Cell& rightCell = at(x + 1, y);
                    double pressureDiff = cell.pressure.y - rightCell.pressure.y;
                    if (pressureDiff > 0.001) {
                        lateralPressure.x += pressureDiff * 0.1; // Pressure flows right
                        rightCell.pressure.x += pressureDiff * 0.1;
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
            std::vector<Vector2d> smoothedPressure(width * height);
            for (uint32_t y = 0; y < height; ++y) {
                for (uint32_t x = 0; x < width; ++x) {
                    Vector2d sum = at(x, y).pressure;
                    int count = 1;

                    // Average with neighbors for smoothing
                    for (int dy = -1; dy <= 1; ++dy) {
                        for (int dx = -1; dx <= 1; ++dx) {
                            if (dx == 0 && dy == 0) continue;
                            
                            Vector2i offset(dx, dy);
                            Vector2i neighborPos(x, y);
                            neighborPos += offset;
                            
                            if (neighborPos.x >= 0 && neighborPos.x < (int)width && 
                                neighborPos.y >= 0 && neighborPos.y < (int)height) {
                                sum += at(neighborPos.x, neighborPos.y).pressure * 0.3; // Reduced weight for neighbors
                                count++;
                            }
                        }
                    }
                    smoothedPressure[y * width + x] = sum / count;
                }
            }

            // Apply smoothed pressure back to cells
            for (uint32_t y = 0; y < height; ++y) {
                for (uint32_t x = 0; x < width; ++x) {
                    at(x, y).pressure = smoothedPressure[y * width + x];
                }
            }
        }
    }
}

Cell& World::at(uint32_t x, uint32_t y)
{
    if (x >= width || y >= height) {
        spdlog::trace(
            "World::at: Attempted to access coordinates ({},{}) but world size is {}x{}",
            x,
            y,
            width,
            height);
        throw std::out_of_range("World::at: Coordinates out of range");
    }
    return cells[coordToIndex(x, y)];
}

const Cell& World::at(uint32_t x, uint32_t y) const
{
    if (x >= width || y >= height) {
        spdlog::trace(
            "World::at: Attempted to access coordinates ({},{}) but world size is {}x{}",
            x,
            y,
            width,
            height);
        throw std::out_of_range("World::at: Coordinates out of range");
    }
    return cells[coordToIndex(x, y)];
}

Cell& World::at(const Vector2i& pos)
{
    return at(static_cast<uint32_t>(pos.x), static_cast<uint32_t>(pos.y));
}

const Cell& World::at(const Vector2i& pos) const
{
    return at(static_cast<uint32_t>(pos.x), static_cast<uint32_t>(pos.y));
}

CellInterface& World::getCellInterface(uint32_t x, uint32_t y)
{
    return at(x, y); // Call the existing at() method
}

const CellInterface& World::getCellInterface(uint32_t x, uint32_t y) const
{
    return at(x, y); // Call the existing at() method  
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

        // Skip moves from truly empty cells only
        if (sourceCell.percentFull() <= 0.0) {
            spdlog::trace("Skipping move from empty cell at ({},{})", move.fromX, move.fromY);
            continue;
        }

        const double originalSourceMass = sourceCell.percentFull();

        // Calculate maximum possible transfer while respecting capacity with safety margin
        const double availableSpace = 1.0 - targetCell.percentFull();
        const double safeAvailableSpace = std::max(0.0, availableSpace - 0.01); // 1% safety margin
        const double totalMass = move.dirtAmount + move.waterAmount;
        const double moveAmount = std::min({
            totalMass,
            originalSourceMass, // Can't move more than we have.
            safeAvailableSpace  // Can't exceed target capacity with safety margin.
        });

        spdlog::trace(
            "Transfer: from=({},{}) to=({},{}) source_v=({},{}) target_v=({},{})",
            move.fromX,
            move.fromY,
            move.toX,
            move.toY,
            sourceCell.v.x,
            sourceCell.v.y,
            targetCell.v.x,
            targetCell.v.y);

        // Validate cell states before transfer
        sourceCell.validateState("source before transfer");
        targetCell.validateState("target before transfer");

        // Assert that transfer calculations are valid
        ASSERT(std::isfinite(availableSpace), "Available space is NaN or infinite");
        ASSERT(std::isfinite(totalMass), "Total mass is NaN or infinite");
        ASSERT(std::isfinite(moveAmount), "Move amount is NaN or infinite");

        // --- ELASTIC COLLISION RESPONSE (only if both cells have mass) ---
        if (sourceCell.percentFull() > MIN_MATTER_THRESHOLD
            && targetCell.percentFull() > MIN_MATTER_THRESHOLD) {
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
        const double moveFraction __attribute__((unused)) = originalSourceMass > 0 ? moveAmount / originalSourceMass : 0.0;

        // Calculate actual amounts to move based on proportions
        const double dirtProportion =
            originalSourceMass > 0 ? sourceCell.dirt / originalSourceMass : 0.0;
        const double waterProportion =
            originalSourceMass > 0 ? sourceCell.water / originalSourceMass : 0.0;

        // Assert that proportions are valid
        ASSERT(std::isfinite(dirtProportion), "Dirt proportion is NaN or infinite");
        ASSERT(std::isfinite(waterProportion), "Water proportion is NaN or infinite");
        ASSERT(
            dirtProportion >= 0.0 && dirtProportion <= 1.0, "Dirt proportion out of range [0,1]");
        ASSERT(
            waterProportion >= 0.0 && waterProportion <= 1.0,
            "Water proportion out of range [0,1]");

        // Apply fragmentation: reduce dirt transfer by fragmentation factor
        // Higher fragmentation factor means more dirt stays behind
        const double dirtMoveAmount =
            moveAmount * dirtProportion * (1.0 - DIRT_FRAGMENTATION_FACTOR);
        const double waterMoveAmount =
            moveAmount * waterProportion; // Water not affected by fragmentation

        const double actualDirt = std::min(dirtMoveAmount, sourceCell.dirt);
        const double actualWater = std::min(waterMoveAmount, sourceCell.water);

        // Assert that actual transfer amounts are valid
        ASSERT(std::isfinite(actualDirt) && actualDirt >= 0.0, "Actual dirt amount invalid");
        ASSERT(std::isfinite(actualWater) && actualWater >= 0.0, "Actual water amount invalid");

        // Update source cell
        spdlog::trace(
            "Before source update: dirt={} water={} total={}",
            sourceCell.dirt,
            sourceCell.water,
            sourceCell.percentFull());
        spdlog::trace("Removing: actualDirt={} actualWater={}", actualDirt, actualWater);
        sourceCell.update(sourceCell.dirt - actualDirt, sourceCell.com, sourceCell.v);
        sourceCell.water -= actualWater;
        spdlog::trace(
            "After source update: dirt={} water={} total={}",
            sourceCell.dirt,
            sourceCell.water,
            sourceCell.percentFull());
        sourceCell.validateState("source after update");

        // Update target cell
        const double oldTargetMass = targetCell.percentFull();
        Vector2d expectedCom = move.comOffset;
        ASSERT(
            std::isfinite(oldTargetMass) && std::isfinite(expectedCom.x)
                && std::isfinite(expectedCom.y),
            "Target update values invalid");

        // Update target cell's COM using weighted average
        if (oldTargetMass == 0.0) {
            // First mass in cell, use the expected COM and transfer velocity
            spdlog::trace("Target cell empty, adding: dirt={} water={}", actualDirt, actualWater);
            // Safely add dirt with capacity checking - return excess to source
            double actualDirtAdded = targetCell.safeAddMaterial(targetCell.dirt, actualDirt);
            double excessDirt = actualDirt - actualDirtAdded;
            if (excessDirt > 0.0) {
                sourceCell.dirt += excessDirt; // Return excess to source
                spdlog::trace("Returned excess dirt to source: {}", excessDirt);
            }
            // Safely add water with capacity checking - return excess to source
            double actualWaterAdded = targetCell.safeAddMaterial(targetCell.water, actualWater);
            double excessWater = actualWater - actualWaterAdded;
            if (excessWater > 0.0) {
                sourceCell.water += excessWater; // Return excess to source
                spdlog::trace("Returned excess water to source: {}", excessWater);
            }
            // Update COM and velocity after material additions
            if (targetCell.percentFull() > 0.0) {
                targetCell.com = expectedCom;
                targetCell.v = sourceCell.v;
            }
            spdlog::trace(
                "Target after update: dirt={} water={} total={}",
                targetCell.dirt,
                targetCell.water,
                targetCell.percentFull());
        }
        else {
            // Weighted average of existing COM and the new COM based on mass.
            // This preserves both mass and energy in the system.
            const double newMass = actualDirt + actualWater;
            Vector2d newCom = (targetCell.com * oldTargetMass + expectedCom * newMass)
                / (oldTargetMass + newMass);

            // Assert weighted average calculation is valid
            ASSERT(
                std::isfinite(newMass) && std::isfinite(newCom.x) && std::isfinite(newCom.y),
                "Weighted average calculation invalid");
            ASSERT(oldTargetMass + newMass > 0.0, "Total mass is zero during weighted average");

            spdlog::trace(
                "Target cell has mass, adding: dirt={} water={}", actualDirt, actualWater);
            // Safely add dirt with capacity checking - return excess to source
            double actualDirtAdded = targetCell.safeAddMaterial(targetCell.dirt, actualDirt);
            double excessDirt = actualDirt - actualDirtAdded;
            if (excessDirt > 0.0) {
                sourceCell.dirt += excessDirt; // Return excess to source
                spdlog::trace("Returned excess dirt to source: {}", excessDirt);
            }
            // Safely add water with capacity checking - return excess to source
            double actualWaterAdded = targetCell.safeAddMaterial(targetCell.water, actualWater);
            double excessWater = actualWater - actualWaterAdded;
            if (excessWater > 0.0) {
                sourceCell.water += excessWater; // Return excess to source
                spdlog::trace("Returned excess water to source: {}", excessWater);
            }
            // Update COM and velocity after material additions
            if (targetCell.percentFull() > 0.0) {
                targetCell.com = newCom;
                // Keep existing velocity for cells with mass
            }
            spdlog::trace(
                "Target after update: dirt={} water={} total={}",
                targetCell.dirt,
                targetCell.water,
                targetCell.percentFull());
        }

        targetCell.validateState("target after update");

        // If the target cell is overfull, log it. This is the root cause.
        if (targetCell.percentFull() > 1.01) {
            spdlog::trace("&&& OVERFULL CELL DETECTED &&&");
            spdlog::trace(
                "  - Cell ({},{}) is now {}% full.",
                move.toX,
                move.toY,
                targetCell.percentFull() * 100);
            spdlog::trace("  - Added dirt: {} water: {}", actualDirt, actualWater);
            spdlog::trace("  - It was {}% full before.", oldTargetMass * 100);
            spdlog::trace("  - Transferred from ({},{})", move.fromX, move.fromY);
        }

        // Update source cell's COM and velocity if any mass remains.
        if (sourceCell.percentFull() > 0.0) {
            // Preserve velocity if no collision occurred
            Vector2d newV = sourceCell.v;
            // Scale COM based on remaining mass
            const double remainingMass __attribute__((unused)) = sourceCell.dirt + sourceCell.water;
            const double movedMassFraction =
                originalSourceMass > 0 ? (actualDirt + actualWater) / originalSourceMass : 0.0;
            Vector2d newCom = sourceCell.com * (1.0 - movedMassFraction);

            // Assert source cell remaining calculations are valid
            ASSERT(
                std::isfinite(remainingMass) && std::isfinite(movedMassFraction)
                    && std::isfinite(newCom.x) && std::isfinite(newCom.y),
                "Source remaining calculations invalid");
            ASSERT(
                movedMassFraction >= 0.0 && movedMassFraction <= 1.001,
                "Move fraction out of range [0,1]"); // Allow for float error

            sourceCell.update(sourceCell.dirt, newCom, newV);
        }
        else {
            sourceCell.update(0.0, Vector2d(0.0, 0.0), Vector2d(0.0, 0.0));
            sourceCell.water = 0.0;
        }

        // Final validation of both cells
        sourceCell.validateState("source final");
        targetCell.validateState("target final");

        // Debug: Log final state
        spdlog::trace(
            "After transfer: source_v=({},{}) target_v=({},{})",
            sourceCell.v.x,
            sourceCell.v.y,
            targetCell.v.x,
            targetCell.v.y);
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
    spdlog::info("Resetting World (RulesA) to empty state");
    
    // Exit time reversal navigation mode to ensure we're working with current state
    currentHistoryIndex = -1;
    hasStoredCurrentState = false;
    
    // Reset simulation state
    simulationTime = 0.0;
    timestep = 0;
    lastSaveTime = 0.0;
    hasUserInputSinceLastSave = false;

    // Mark user input for time reversal
    markUserInput();

    // Clear all cells
    spdlog::info("Clearing {} cells", cells.size());
    for (auto& cell : cells) {
        cell.update(0.0, Vector2d(0.0, 0.0), Vector2d(0.0, 0.0));
        cell.water = 0.0;
        cell.wood = 0.0;
        cell.leaf = 0.0;
        cell.metal = 0.0;
        cell.pressure = Vector2d(0.0, 0.0);
        cell.markDirty();
    }
    
    spdlog::info("World (RulesA) reset complete - world is now empty");
}

void World::setup()
{
    spdlog::info("Setting up World (RulesA) with initial materials");
    
    // First reset to empty state
    reset();
    
    spdlog::info("About to check worldSetup pointer...");
    // Use the world setup strategy to initialize the world
    if (worldSetup) {
        spdlog::info("Calling WorldSetup::setup for World (RulesA)");
        worldSetup->setup(*this);
    } else {
        spdlog::error("WorldSetup is null in World::setup()!");
    }
    
    spdlog::info("World (RulesA) setup complete");
    spdlog::info("DEBUGGING: Total mass after setup = {:.3f}", getTotalMass());
}

void World::addDirtAtPixel(int pixelX, int pixelY)
{
    spdlog::info("WorldA::addDirtAtPixel pixel ({},{}) -> cell ({},{})", 
                 pixelX, pixelY, pixelX / Cell::WIDTH, pixelY / Cell::HEIGHT);
    
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
    } else {
        spdlog::info("WorldA: Pixel ({},{}) -> cell ({},{}) is out of bounds (grid: {}x{})", 
                     pixelX, pixelY, cellX, cellY, width, height);
    }
}

void World::addWaterAtPixel(int pixelX, int pixelY)
{
    spdlog::info("WorldA::addWaterAtPixel pixel ({},{}) -> cell ({},{})", 
                 pixelX, pixelY, pixelX / Cell::WIDTH, pixelY / Cell::HEIGHT);
    
    // Mark user input for time reversal
    markUserInput();

    // Convert pixel coordinates to cell coordinates
    int cellX = pixelX / Cell::WIDTH;
    int cellY = pixelY / Cell::HEIGHT;

    // Check if coordinates are within bounds
    if (cellX >= 0 && cellX < static_cast<int>(width) && cellY >= 0
        && cellY < static_cast<int>(height)) {
        Cell& cell = at(cellX, cellY);
        spdlog::info("WorldA: Adding water to cell ({},{}) - before: dirt={:.3f}, water={:.3f}", 
                     cellX, cellY, cell.dirt, cell.water);
        // Use safeAddMaterial to respect capacity limits
        double actualAdded = cell.safeAddMaterial(cell.water, 1.0);
        cell.markDirty();
        spdlog::info("WorldA: After adding water - dirt={:.3f}, water={:.3f}, actualAdded={:.3f}", 
                     cell.dirt, cell.water, actualAdded);
    } else {
        spdlog::info("WorldA: Pixel ({},{}) -> cell ({},{}) is out of bounds (grid: {}x{})", 
                     pixelX, pixelY, cellX, cellY, width, height);
    }
}

void World::addMaterialAtPixel(int pixelX, int pixelY, MaterialType type, double amount)
{
    spdlog::debug("WorldA::addMaterialAtPixel({}) at pixel ({},{}) with amount {:.3f}", 
                  getMaterialName(type), pixelX, pixelY, amount);
    
    // WorldA mapping: Convert materials to dirt/water equivalents
    switch (type) {
        case MaterialType::DIRT:
        case MaterialType::SAND:   // Sand -> dirt (granular material)
        case MaterialType::WOOD:   // Wood -> dirt (solid material)
        case MaterialType::METAL:  // Metal -> dirt (dense solid)
        case MaterialType::WALL:   // Wall -> dirt (immovable -> dense dirt)
            addDirtAtPixel(pixelX, pixelY);
            break;
            
        case MaterialType::WATER:
        case MaterialType::LEAF:   // Leaf -> water (light material)
            addWaterAtPixel(pixelX, pixelY);
            break;
            
        case MaterialType::AIR:
            // AIR -> no operation (WorldA cannot remove materials easily)
            spdlog::debug("WorldA: AIR material ignored (cannot remove materials)");
            break;
            
        default:
            spdlog::warn("WorldA: Unknown material type {}, defaulting to DIRT", static_cast<int>(type));
            addDirtAtPixel(pixelX, pixelY);
            break;
    }
}

bool World::hasMaterialAtPixel(int pixelX, int pixelY) const
{
    int cellX, cellY;
    pixelToCell(pixelX, pixelY, cellX, cellY);
    
    if (cellX >= 0 && cellX < static_cast<int>(width) && 
        cellY >= 0 && cellY < static_cast<int>(height)) {
        const CellInterface& cell = getCellInterface(cellX, cellY);
        return !cell.isEmpty();
    }
    
    return false;
}

void World::pixelToCell(int pixelX, int pixelY, int& cellX, int& cellY) const
{
    cellX = pixelX / Cell::WIDTH;
    cellY = pixelY / Cell::HEIGHT;
}

Vector2i World::pixelToCell(int pixelX, int pixelY) const
{
    return Vector2i(pixelX / Cell::WIDTH, pixelY / Cell::HEIGHT);
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
    spdlog::info("Starting drag at pixel ({},{})", pixelX, pixelY);

    // Mark user input for time reversal
    markUserInput();

    int cellX, cellY;
    pixelToCell(pixelX, pixelY, cellX, cellY);

    // Check if coordinates are within bounds
    if (cellX >= 0 && cellX < static_cast<int>(width) && cellY >= 0
        && cellY < static_cast<int>(height)) {
        Cell& cell = at(cellX, cellY);

        // Only start dragging if there's dirt to drag
        if (cell.dirt > MIN_MATTER_THRESHOLD) {
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
        spdlog::debug(
            "Drag Update - Cell: ({},{}) COM: ({:.2f},{:.2f}) Recent positions: {} Current "
            "velocity: ({:.2f},{:.2f})",
            cellX,
            cellY,
            draggedCom.x,
            draggedCom.y,
            recentPositions.size(),
            draggedVelocity.x,
            draggedVelocity.y);
    }
}

void World::endDragging(int pixelX, int pixelY)
{
    if (!isDragging) return;

    spdlog::info("Ending drag at pixel ({},{})", pixelX, pixelY);

    // Mark user input for time reversal
    markUserInput();

    int cellX, cellY;
    pixelToCell(pixelX, pixelY, cellX, cellY);

    // Debug: Print recent positions before calculating velocity
    spdlog::trace("Release Debug:");
    spdlog::trace("Recent positions: ");
    for (const auto& pos : recentPositions) {
        spdlog::trace("({},{}) ", pos.first, pos.second);
    }
    spdlog::trace("");

    // Calculate final COM based on cursor position within cell
    double subCellX = (pixelX % Cell::WIDTH) / static_cast<double>(Cell::WIDTH);
    double subCellY = (pixelY % Cell::HEIGHT) / static_cast<double>(Cell::HEIGHT);
    // Map from [0,1] to [-1,1]
    draggedCom = Vector2d(subCellX * 2.0 - 1.0, subCellY * 2.0 - 1.0);
    spdlog::trace("Final COM before placement: ({},{})", draggedCom.x, draggedCom.y);

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
                spdlog::trace("Step {} delta: ({},{})", i, dx, dy);
            }
            avgDx /= (recentPositions.size() - 1);
            avgDy /= (recentPositions.size() - 1);
            // Scale by cell size and a factor for better feel
            draggedVelocity = Vector2d(avgDx * Cell::WIDTH * 2.0, avgDy * Cell::HEIGHT * 2.0);

            spdlog::trace(
                "Final velocity before placement: ({},{})", draggedVelocity.x, draggedVelocity.y);
        }
        else {
            spdlog::trace("Not enough positions for velocity calculation");
        }

        // Queue up the drag end state for processing in advanceTime
        pendingDragEnd.hasPendingEnd = true;
        pendingDragEnd.cellX = cellX;
        pendingDragEnd.cellY = cellY;
        pendingDragEnd.dirt = draggedDirt;
        pendingDragEnd.velocity = draggedVelocity;
        pendingDragEnd.com = draggedCom;

        spdlog::trace(
            "Queued drag end at ({},{}) with velocity ({},{}) and COM ({},{})",
            cellX,
            cellY,
            draggedVelocity.x,
            draggedVelocity.y,
            draggedCom.x,
            draggedCom.y);
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

void World::resizeGrid(uint32_t newWidth, uint32_t newHeight)
{
    // Don't resize if dimensions haven't changed
    if (newWidth == width && newHeight == height) {
        spdlog::debug("Resize requested but dimensions unchanged: {}x{}", width, height);
        return;
    }

    spdlog::info(
        "Resizing World grid: {}x{} -> {}x{}",
        width,
        height,
        newWidth,
        newHeight);

    // Mark this as user input for time reversal (preserving history)
    markUserInput();

    // Try bilinear interpolation first for smoother rescaling
    if (WorldInterpolationTool::resizeWorldWithBilinearFiltering(*this, newWidth, newHeight)) {
        spdlog::info("WorldA bilinear resize completed successfully");
        return;
    }
    
    spdlog::warn("Bilinear resize failed for WorldA, falling back to WorldSetup method");

    // Fallback to original WorldSetup-based interpolation
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
    state.totalMass = getTotalMass(); // Use the getter
    state.removedMass = removedMass;
    state.timestamp = simulationTime; // Capture current simulation time

    stateHistory.push_back(std::move(state));

    // Limit history size to prevent memory issues
    if (stateHistory.size() > MAX_HISTORY_SIZE) {
        stateHistory.erase(stateHistory.begin());
    }

    spdlog::trace(
        "Saved world state at time {}s. History size: {}", simulationTime, stateHistory.size());
}

void World::goBackward()
{
    if (!canGoBackward()) return;

    // If we're at the 'present', save the current state before moving back
    if (currentHistoryIndex == -1) {
        WorldState currentLiveState;
        currentLiveState.cells = cells;
        currentLiveState.width = width;
        currentLiveState.height = height;
        currentLiveState.cellWidth = Cell::WIDTH;
        currentLiveState.cellHeight = Cell::HEIGHT;
        currentLiveState.timestep = timestep;
        currentLiveState.totalMass = getTotalMass(); // Use the getter
        currentLiveState.removedMass = removedMass;
        currentLiveState.timestamp = simulationTime;
        stateHistory.push_back(std::move(currentLiveState));
        currentHistoryIndex = stateHistory.size() - 2;
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

    spdlog::trace(
        "Restored world state from history index: {} (timestep: {}, time: {}s)",
        currentHistoryIndex,
        timestep,
        simulationTime);
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
            spdlog::trace("Restored captured current live state");
        }
        return;
    }

    // Restore the state using the helper method that handles grid size changes
    const WorldState& state = stateHistory[currentHistoryIndex];
    restoreWorldState(state);

    spdlog::trace(
        "Restored world state from history index: {} (timestep: {}, time: {}s)",
        currentHistoryIndex,
        timestep,
        simulationTime);
}

void World::restoreWorldState(const WorldState& state)
{
    // Restore cell size first (affects grid calculations)
    if (Cell::WIDTH != state.cellWidth || Cell::HEIGHT != state.cellHeight) {
        spdlog::trace(
            "Cell size changed from {}x{} to {}x{} during restore",
            Cell::WIDTH,
            Cell::HEIGHT,
            state.cellWidth,
            state.cellHeight);
    }
    Cell::WIDTH = state.cellWidth;
    Cell::HEIGHT = state.cellHeight;

    // Check if grid size has changed
    if (state.width != width || state.height != height) {
        spdlog::trace(
            "Grid size changed from {}x{} to {}x{} during restore",
            width,
            height,
            state.width,
            state.height);

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
    removedMass = state.removedMass;
    simulationTime = state.timestamp;

    // Mark all cells as needing redraw
    for (auto& cell : cells) {
        cell.markDirty();
    }
}

// World type management implementation
WorldType World::getWorldType() const
{
    return WorldType::RulesA;
}

void World::preserveState(::WorldState& state) const
{
    // Initialize state with current world properties
    state.initializeGrid(width, height);
    state.timescale = timescale;
    state.timestep = timestep;
    
    // Copy physics parameters
    state.gravity = gravity;
    state.elasticity_factor = ELASTICITY_FACTOR;
    state.pressure_scale = pressureScale;
    state.dirt_fragmentation_factor = DIRT_FRAGMENTATION_FACTOR;
    state.water_pressure_threshold = waterPressureThreshold;
    
    // Copy world setup flags
    state.left_throw_enabled = isLeftThrowEnabled();
    state.right_throw_enabled = isRightThrowEnabled();
    state.lower_right_quadrant_enabled = isLowerRightQuadrantEnabled();
    state.walls_enabled = areWallsEnabled();
    state.rain_rate = getRainRate();
    
    // Copy time reversal state
    state.time_reversal_enabled = timeReversalEnabled;
    
    // Copy control flags
    state.add_particles_enabled = addParticlesEnabled;
    state.cursor_force_enabled = cursorForceEnabled;
    
    // Convert Cell data to WorldState::CellData
    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            const Cell& cell = at(x, y);
            
            // Calculate total mass and determine dominant material
            double totalMass = cell.dirt + cell.water;
            MaterialType dominantMaterial = MaterialType::AIR;
            
            if (totalMass > MIN_MATTER_THRESHOLD) {
                if (cell.dirt > cell.water) {
                    dominantMaterial = MaterialType::DIRT;
                } else if (cell.water > 0.0) {
                    dominantMaterial = MaterialType::WATER;
                }
            }
            
            // Create CellData with converted information
            ::WorldState::CellData cellData(
                totalMass,
                dominantMaterial,
                cell.v,
                cell.com
            );
            
            state.setCellData(x, y, cellData);
        }
    }
    
    spdlog::info("World state preserved: {}x{} grid with {} total mass", 
                width, height, getTotalMass());
}

void World::restoreState(const ::WorldState& state)
{
    spdlog::info("Restoring World state from {}x{} grid", state.width, state.height);
    
    // Resize grid if necessary
    if (state.width != width || state.height != height) {
        resizeGrid(state.width, state.height);
    }
    
    // Restore physics parameters
    timescale = state.timescale;
    timestep = state.timestep;
    gravity = state.gravity;
    ELASTICITY_FACTOR = state.elasticity_factor;
    pressureScale = state.pressure_scale;
    DIRT_FRAGMENTATION_FACTOR = state.dirt_fragmentation_factor;
    waterPressureThreshold = state.water_pressure_threshold;
    
    // Restore world setup flags
    setLeftThrowEnabled(state.left_throw_enabled);
    setRightThrowEnabled(state.right_throw_enabled);
    setLowerRightQuadrantEnabled(state.lower_right_quadrant_enabled);
    setWallsEnabled(state.walls_enabled);
    setRainRate(state.rain_rate);
    
    // Restore time reversal state
    timeReversalEnabled = state.time_reversal_enabled;
    
    // Restore control flags
    addParticlesEnabled = state.add_particles_enabled;
    cursorForceEnabled = state.cursor_force_enabled;
    
    // Convert WorldState::CellData back to Cell data
    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            const ::WorldState::CellData& cellData = state.getCellData(x, y);
            Cell& cell = at(x, y);
            
            // Convert from pure material back to mixed dirt/water
            double dirtAmount = 0.0;
            double waterAmount = 0.0;
            
            if (cellData.material_mass > MIN_MATTER_THRESHOLD) {
                switch (cellData.dominant_material) {
                    case MaterialType::DIRT:
                    case MaterialType::SAND:
                    case MaterialType::WOOD:
                    case MaterialType::METAL:
                    case MaterialType::LEAF:
                    case MaterialType::WALL:
                        // Convert solid materials to dirt
                        // WorldB mass = fill_ratio * density, so convert back: dirt = mass / density
                        {
                            const MaterialProperties& props = getMaterialProperties(cellData.dominant_material);
                            dirtAmount = props.density > 0.0 ? cellData.material_mass / props.density : cellData.material_mass;
                            dirtAmount = std::min(1.0, dirtAmount); // Clamp to valid range
                        }
                        break;
                    case MaterialType::WATER:
                        // Convert water to water
                        // WorldB mass = fill_ratio * density, so convert back: water = mass / density
                        {
                            const MaterialProperties& props = getMaterialProperties(MaterialType::WATER);
                            waterAmount = props.density > 0.0 ? cellData.material_mass / props.density : cellData.material_mass;
                            waterAmount = std::min(1.0, waterAmount); // Clamp to valid range
                        }
                        break;
                    case MaterialType::AIR:
                    default:
                        // Empty cell
                        break;
                }
            }
            
            // Update cell with converted data
            cell.update(dirtAmount, cellData.com, cellData.velocity);
            cell.water = waterAmount;
            cell.pressure = Vector2d(0.0, 0.0);  // Reset pressure since CellData no longer stores it
            cell.markDirty();
        }
    }
    
    // Clear history since we've restored from external state
    clearHistory();
    
    spdlog::info("World state restored: {} total mass", getTotalMass());
}

std::string World::toAsciiDiagram() const
{
    std::ostringstream diagram;
    
    // Add top border (2 chars per cell + 1 space between each cell, except last)
    diagram << "+";
    for (uint32_t x = 0; x < width; ++x) {
        diagram << "--";
        if (x < width - 1) {
            diagram << "-";  // Space between cells
        }
    }
    diagram << "+\n";
    
    // Iterate through each row from top to bottom
    for (uint32_t y = 0; y < height; ++y) {
        diagram << "|";  // Left border
        
        // Add each cell's ASCII representation (2 characters each) with spaces between
        for (uint32_t x = 0; x < width; ++x) {
            const Cell& cell = cells[coordToIndex(x, y)];
            diagram << cell.toAsciiCharacter();
            if (x < width - 1) {
                diagram << " ";  // Space between cells
            }
        }
        
        diagram << "|\n";  // Right border and newline
    }
    
    // Add bottom border (2 chars per cell + 1 space between each cell, except last)
    diagram << "+";
    for (uint32_t x = 0; x < width; ++x) {
        diagram << "--";
        if (x < width - 1) {
            diagram << "-";  // Space between cells
        }
    }
    diagram << "+\n";
    
    return diagram.str();
}
