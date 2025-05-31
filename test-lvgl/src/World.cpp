#include "World.h"

#include <algorithm>
#include <iostream>
#include <random>
#include <stdexcept>

namespace {
void addParticles(World& world, uint32_t timestep)
{
    // Add a particle at the center-top every 100th timestep.
    if (timestep == 20) {
        uint32_t centerX = world.getWidth() / 2;
        Cell& cell = world.at(centerX, 1); // 1 to be just below the top wall.
        cell.dirty = 1.0;
        cell.v = Vector2d(0.0, 0.0);
        cell.com = Vector2d(0.0, 0.0);
    }
}
} // namespace

World::World(uint32_t width, uint32_t height, lv_obj_t* draw_area)
    : width(width), height(height), cells(width * height), draw_area(draw_area)
{}

void World::advanceTime(uint32_t deltaTimeMs)
{
    if (addParticlesEnabled) {
        addParticles(*this, timestep++);
    }
    else {
        timestep++;
    }

    const double gravity = 9.81; // m/s^2.
    const double timeStep =
        deltaTimeMs / 1000.0 * timescale; // Convert to seconds and apply timescale.

    struct DirtMove {
        uint32_t fromX;
        uint32_t fromY;
        uint32_t toX;
        uint32_t toY;
        double amount;       // Original amount of dirt to move.
        double actualAmount; // Actual amount moved after capacity checks.
        Vector2d comOffset;  // COM offset to apply to the target cell.
    };

    std::vector<DirtMove> moves;

    // First pass: collect potential moves.
    for (uint32_t y = 0; y < height; y++) {
        for (uint32_t x = 0; x < width; x++) {
            Cell& cell = at(x, y);

            // Skip empty cells or cells with dirt below threshold.
            if (cell.dirty < MIN_DIRT_THRESHOLD) {
                // Track the mass being removed.
                removedMass += cell.dirty;
                // Reset them to initial conditions.
                cell.dirty = 0.0;
                cell.com = Vector2d(0.0, 0.0);
                cell.v = Vector2d(0.0, 0.0);
                continue;
            }

            // Apply gravity.
            cell.v.y += gravity * timeStep;

            // Calculate predicted COM position after this timestep.
            Vector2d predictedCom = cell.com + cell.v * timeStep;

            // Calculate if predicted COM will be in neighboring cell space or moving toward it.
            bool shouldTransferX = false;
            bool shouldTransferY = false;
            int targetX = x;
            int targetY = y;
            Vector2d comOffset;

            // Check horizontal transfer.
            if (predictedCom.x > 1.0
                || (cell.v.x > 0.0
                    && cell.com.x > 0.0)) { // Will move into or moving toward right neighbor.
                shouldTransferX = true;
                targetX = x + 1;
                comOffset.x = predictedCom.x > 1.0
                    ? predictedCom.x - 2.0
                    : -1.0; // Use exact position if crossed boundary.
            }
            else if (
                predictedCom.x < -1.0
                || (cell.v.x < 0.0
                    && cell.com.x < 0.0)) { // Will move into or moving toward left neighbor.
                shouldTransferX = true;
                targetX = x - 1;
                comOffset.x = predictedCom.x < -1.0
                    ? predictedCom.x + 2.0
                    : 1.0; // Use exact position if crossed boundary.
            }

            // Check vertical transfer.
            if (predictedCom.y > 1.0
                || (cell.v.y > 0.0
                    && cell.com.y > 0.0)) { // Will move into or moving toward bottom neighbor.
                shouldTransferY = true;
                targetY = y + 1;
                comOffset.y = predictedCom.y > 1.0
                    ? predictedCom.y - 2.0
                    : -1.0; // Use exact position if crossed boundary.
            }
            else if (
                predictedCom.y < -1.0
                || (cell.v.y < 0.0
                    && cell.com.y < 0.0)) { // Will move into or moving toward top neighbor.
                shouldTransferY = true;
                targetY = y - 1;
                comOffset.y = predictedCom.y < -1.0
                    ? predictedCom.y + 2.0
                    : 1.0; // Use exact position if crossed boundary.
            }

            // Only queue moves if there's dirt to transfer and predicted COM will be in or moving
            // toward neighboring cell space.
            if (cell.dirty > 0.0 && (shouldTransferX || shouldTransferY)) {
                // Only queue move if target is within bounds.
                if (targetX >= 0 && targetX < width && targetY >= 0 && targetY < height) {
                    Cell& targetCell = at(targetX, targetY);
                    // Calculate how much space is available in target cell.
                    double availableSpace = 1.0 - targetCell.dirty;

                    // Calculate transfer amount based on velocity and available space.
                    double velocityFactor = cell.v.length() * timeStep;
                    double moveAmount = std::min({
                        cell.dirty,                  // Can't move more than exists
                        cell.dirty * velocityFactor, // Velocity-based amount
                        availableSpace               // Can't exceed target cell's capacity
                    });

                    // Only queue move if there's actually dirt to move
                    if (moveAmount > 0.0) {
                        moves.push_back({ x,
                                          y,
                                          static_cast<uint32_t>(targetX),
                                          static_cast<uint32_t>(targetY),
                                          moveAmount,
                                          0.0,
                                          comOffset });
                    }
                }
            }

            // Update actual COM position after transfers are queued.
            cell.com = predictedCom;
            cell.com.x = std::clamp(cell.com.x, -1.0, 1.0);
            cell.com.y = std::clamp(cell.com.y, -1.0, 1.0);
        }
    }

    // Shuffle moves to avoid bias.
    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(moves.begin(), moves.end(), g);

    // Second pass: apply valid moves.
    for (const auto& move : moves) {
        Cell& sourceCell = at(move.fromX, move.fromY);
        Cell& targetCell = at(move.toX, move.toY);

        // Only move if target cell isn't full and source has dirt to give
        if (targetCell.dirty < 1.0 && sourceCell.dirty > 0.0) {
            // Calculate maximum possible transfer while respecting capacity
            const double availableSpace = 1.0 - targetCell.dirty;
            const double moveAmount = std::min({
                move.amount,
                sourceCell.dirty, // Can't move more than we have
                availableSpace    // Can't exceed target capacity
            });

            if (moveAmount <= 0.0) {
                continue;
            }

            // Calculate the fraction being moved
            const double moveFraction = moveAmount / sourceCell.dirty;

            // Update source cell
            sourceCell.dirty -= moveAmount;

            // Update target cell
            const double oldTargetMass = targetCell.dirty;
            targetCell.dirty += moveAmount;

            // When dirt moves downward, set its initial COM near the top of the target cell
            if (move.fromY < move.toY) { // Moving downward
                if (oldTargetMass == 0.0) {
                    // First dirt in cell, place at top
                    targetCell.com = Vector2d(0.0, -0.9);
                }
                else {
                    // Weighted average with new dirt at top
                    targetCell.com =
                        (targetCell.com * oldTargetMass + Vector2d(0.0, -0.9) * moveAmount)
                        / targetCell.dirty;
                }
            }
            else if (move.fromY > move.toY) { // Moving upward
                if (oldTargetMass == 0.0) {
                    // First dirt in cell, place at bottom
                    targetCell.com = Vector2d(0.0, 0.9);
                }
                else {
                    // Weighted average with new dirt at bottom
                    targetCell.com =
                        (targetCell.com * oldTargetMass + Vector2d(0.0, 0.9) * moveAmount)
                        / targetCell.dirty;
                }
            }
            else { // Horizontal movement
                if (oldTargetMass == 0.0) {
                    // First dirt in cell, maintain horizontal position
                    targetCell.com = Vector2d(move.comOffset.x, 0.0);
                }
                else {
                    // Weighted average maintaining horizontal position
                    targetCell.com = (targetCell.com * oldTargetMass
                                      + Vector2d(move.comOffset.x, 0.0) * moveAmount)
                        / targetCell.dirty;
                }
            }

            // Transfer momentum
            if (targetCell.dirty > 0.0) { // Prevent division by zero
                targetCell.v =
                    (targetCell.v * oldTargetMass + sourceCell.v * moveAmount) / targetCell.dirty;
            }

            // Update source cell's COM and velocity if any dirt remains
            if (sourceCell.dirty > 0.0) {
                sourceCell.v *= (1.0 - moveFraction);
                // Adjust source COM to account for removed mass
                sourceCell.com = sourceCell.com * (1.0 - moveFraction);
            }
            else {
                sourceCell.v = Vector2d(0.0, 0.0);
                sourceCell.com = Vector2d(0.0, 0.0);
            }
        }
    }

    // Update total mass after all moves are complete.
    totalMass = 0.0;
    for (uint32_t y = 0; y < height; y++) {
        for (uint32_t x = 0; x < width; x++) {
            totalMass += at(x, y).dirty;
        }
    }
}

Cell& World::at(uint32_t x, uint32_t y)
{
    if (x >= width || y >= height) {
        throw std::out_of_range("World::at: Coordinates out of range");
    }
    return cells[coordToIndex(x, y)];
}

const Cell& World::at(uint32_t x, uint32_t y) const
{
    if (x >= width || y >= height) {
        throw std::out_of_range("World::at: Coordinates out of range");
    }
    return cells[coordToIndex(x, y)];
}

size_t World::coordToIndex(uint32_t x, uint32_t y) const
{
    return y * width + x;
}

void World::draw()
{
    if (draw_area == nullptr) {
        return;
    }

    for (uint32_t y = 0; y < height; y++) {
        for (uint32_t x = 0; x < width; x++) {
            at(x, y).draw(draw_area, x, y);
        }
    }
}

uint32_t World::getWidth() const
{
    return width;
}

uint32_t World::getHeight() const
{
    return height;
}

void World::fillWithDirt()
{
    for (uint32_t y = 0; y < height; y++) {
        for (uint32_t x = 0; x < width; x++) {
            at(x, y).dirty = 0.5;
            at(x, y).v = Vector2d(1, 0.0);
        }
    }
}

void World::makeWalls()
{
    for (uint32_t x = 0; x < width; x++) {
        //        at(x, 0).dirty = 1.0;
        at(x, height - 1).dirty = 1.0;
    }
    for (uint32_t y = 0; y < height; y++) {
        at(0, y).dirty = 1.0;
        at(width - 1, y).dirty = 1.0;
    }
}

void World::reset()
{
    timestep = 0;
    cells.clear();
    cells.resize(width * height);
    makeWalls();
}
