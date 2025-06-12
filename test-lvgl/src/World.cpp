#include "World.h"
#include "Cell.h"
#include "SimulatorUI.h"
#include "Timers.h"
#include "WorldRules.h"
#include "WorldSetup.h"
#include "spdlog/spdlog.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>
#include <random>
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


World::World(uint32_t width, uint32_t height, lv_obj_t* draw_area)
    : width(width), height(height), cells(width * height), draw_area(draw_area)
{
    spdlog::info("Creating World: {}x{} grid ({} total cells)", width, height, cells.size());

    // Initialize timers
    timers.startTimer("total_simulation");

    // Create configurable world setup
    worldSetup = std::make_unique<ConfigurableWorldSetup>();

    // Initialize default physics rules
    worldRules_ = createWorldRules("RulesA");
}

World::~World()
{
    spdlog::info("Destroying World: {}x{} grid", width, height);

    // Stop the total simulation timer and dump stats
    timers.stopTimer("total_simulation");
    timers.dumpTimerStats();
}

void World::advanceTime(const double deltaTimeSeconds)
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
    if (worldRules_) {
        spdlog::trace("Running physics rules: {}", worldRules_->getName());
        worldRules_->updatePressures(*this, deltaTimeSeconds);
        worldRules_->applyPressureForces(*this, deltaTimeSeconds);
    }

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
            if (worldRules_) {
                worldRules_->applyPhysics(cell, x, y, deltaTimeSeconds, *this);
            }

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
            if (worldRules_) {
                worldRules_->calculateTransferDirection(
                    cell,
                    shouldTransferX,
                    shouldTransferY,
                    targetX,
                    targetY,
                    comOffset,
                    x,
                    y,
                    *this);
            }

            bool transferOccurred = false;
            bool attemptedTransfer = shouldTransferX || shouldTransferY;

            if (attemptedTransfer) {
                const double totalMass = cell.dirt + cell.water;
                bool isTargetInBounds = worldRules_ ? WorldRules::isWithinBounds(targetX, targetY, *this) : false;

                // Try diagonal transfer first if both X and Y are needed and target is valid
                if (shouldTransferX && shouldTransferY && isTargetInBounds) {
                    transferOccurred = worldRules_ ?
                        worldRules_->attemptTransfer(cell, x, y, targetX, targetY, comOffset, totalMass, *this) : false;
                }

                // If no diagonal transfer, or it wasn't attempted, try cardinal directions
                if (!transferOccurred) {
                    // GRAVITY PRIORITY: For dirt particles, prioritize downward movement over
                    // horizontal This prevents dirt from getting "stuck" horizontally when it
                    // should be falling
                    bool isDirtCell = cell.dirt > MIN_DIRT_THRESHOLD;
                    bool isDownwardTransfer = shouldTransferY && targetY > static_cast<int>(y);

                    if (isDirtCell && isDownwardTransfer) {
                        // Prioritize downward transfer for dirt (gravity wins)
                        if (worldRules_ && WorldRules::isWithinBounds(x, targetY, *this)) {
                            Vector2d yComOffset = comOffset;
                            yComOffset.x = cell.com.x; // Keep original X component
                            transferOccurred = worldRules_ ?
                                worldRules_->attemptTransfer(cell, x, y, x, targetY, yComOffset, totalMass, *this) : false;
                        }
                        // Try horizontal only if downward failed
                        if (!transferOccurred && shouldTransferX && worldRules_ && WorldRules::isWithinBounds(targetX, y, *this)) {
                            Vector2d xComOffset = comOffset;
                            xComOffset.y = cell.com.y; // Keep original Y component
                            transferOccurred = worldRules_ ?
                                worldRules_->attemptTransfer(cell, x, y, targetX, y, xComOffset, totalMass, *this) : false;
                        }
                    }
                    else {
                        // For water or upward movement, use original priority (horizontal first)
                        if (shouldTransferX && worldRules_ && WorldRules::isWithinBounds(targetX, y, *this)) {
                            Vector2d xComOffset = comOffset;
                            xComOffset.y = cell.com.y; // Keep original Y component
                            transferOccurred = worldRules_ ?
                                worldRules_->attemptTransfer(cell, x, y, targetX, y, xComOffset, totalMass, *this) : false;
                        }
                        // Try Y transfer if X transfer didn't occur or wasn't needed
                        if (!transferOccurred && shouldTransferY && worldRules_ && WorldRules::isWithinBounds(x, targetY, *this)) {
                            Vector2d yComOffset = comOffset;
                            yComOffset.x = cell.com.x; // Keep original X component
                            transferOccurred = worldRules_ ?
                                worldRules_->attemptTransfer(cell, x, y, x, targetY, yComOffset, totalMass, *this) : false;
                        }
                    }
                }

                // If a transfer was attempted but failed, handle the collision/blockage
                if (!transferOccurred && worldRules_) {
                    worldRules_->handleCollision(
                        cell, x, y, targetX, targetY, shouldTransferX, shouldTransferY, *this);
                }
            }

            // If no transfer was attempted, update COM internally
            if (!attemptedTransfer) {
                cell.update(cell.dirt, predictedCom, cell.v);
                if (worldRules_) {
                    worldRules_->checkExcessiveDeflectionReflection(cell, *this);
                }
            }
        }
    }
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

void World::setWorldRules(std::unique_ptr<WorldRules> rules)
{
    worldRules_ = std::move(rules);
    spdlog::info("World physics rules changed to: {}", worldRules_->getName());
}

void World::setGravity(double g)
{
    if (worldRules_) worldRules_->setGravity(g);
}

void World::setElasticityFactor(double e)
{
    if (worldRules_) worldRules_->setElasticityFactor(e);
}

void World::setPressureScale(double scale)
{
    if (worldRules_) worldRules_->setPressureScale(scale);
}

void World::setWaterPressureThreshold(double threshold)
{
    if (worldRules_) worldRules_->setWaterPressureThreshold(threshold);
}

double World::getWaterPressureThreshold() const
{
    return worldRules_ ? worldRules_->getWaterPressureThreshold() : 0.0004;
}

void World::setDirtFragmentationFactor(double factor)
{
    if (worldRules_) worldRules_->setDirtFragmentationFactor(factor);
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
                double elasticity = worldRules_ ? worldRules_->getElasticityFactor() : 0.8;
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
        const double moveFraction = originalSourceMass > 0 ? moveAmount / originalSourceMass : 0.0;

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
            moveAmount * dirtProportion * (1.0 - (worldRules_ ? worldRules_->getDirtFragmentationFactor() : 0.0));
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
            targetCell.update(targetCell.dirt + actualDirt, expectedCom, sourceCell.v);
            // Safely add water with capacity checking - return excess to source
            double actualWaterAdded = targetCell.safeAddMaterial(targetCell.water, actualWater);
            double excessWater = actualWater - actualWaterAdded;
            if (excessWater > 0.0) {
                sourceCell.water += excessWater; // Return excess to source
                spdlog::trace("Returned excess water to source: {}", excessWater);
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
            targetCell.update(targetCell.dirt + actualDirt, newCom, targetCell.v);
            // Safely add water with capacity checking - return excess to source
            double actualWaterAdded = targetCell.safeAddMaterial(targetCell.water, actualWater);
            double excessWater = actualWater - actualWaterAdded;
            if (excessWater > 0.0) {
                sourceCell.water += excessWater; // Return excess to source
                spdlog::trace("Returned excess water to source: {}", excessWater);
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
            const double remainingMass = sourceCell.dirt + sourceCell.water;
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

void World::resizeGrid(uint32_t newWidth, uint32_t newHeight, bool clearHistoryFlag)
{
    // Don't resize if dimensions haven't changed
    if (newWidth == width && newHeight == height) {
        spdlog::debug("Resize requested but dimensions unchanged: {}x{}", width, height);
        return;
    }

    spdlog::info(
        "Resizing World grid: {}x{} -> {}x{} (clearHistory={})",
        width,
        height,
        newWidth,
        newHeight,
        clearHistoryFlag);

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
