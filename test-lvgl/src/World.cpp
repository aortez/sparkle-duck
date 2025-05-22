#include "World.h"

#include <algorithm>
#include <stdexcept>
#include <random>

World::World(uint32_t width, uint32_t height, lv_obj_t* draw_area)
    : width(width), height(height), cells(width * height), draw_area(draw_area)
{}

void World::advanceTime(uint32_t deltaTimeMs)
{
    const double gravity = 9.81;                  // m/s^2.
    const double timeStep = deltaTimeMs / 1000.0 * timescale; // Convert to seconds and apply timescale.

    struct DirtMove
    {
        uint32_t fromX;
        uint32_t fromY;
        uint32_t toX;
        uint32_t toY;
        double amount;        // Original amount of dirt to move.
        double actualAmount;  // Actual amount moved after capacity checks.
        Vector2d comOffset;   // COM offset to apply to target cell.
    };

    std::vector<DirtMove> moves;

    // First pass: collect potential moves.
    for (uint32_t y = 0; y < height; y++) {
        for (uint32_t x = 0; x < width; x++) {
            Cell& cell = at(x, y);

            // Skip empty cells.
            if (cell.dirty < 0.01) {
            	// Reset them to initial conditions.
              cell.dirty = 0.0;
              cell.com = Vector2d(0.0, 0.0);
              cell.v = Vector2d(0.0, 0.0);
            	continue;
            }

            // Apply gravity.
            cell.v.y += gravity * timeStep;

            // Move internally and clamp to cell boundaries.
            cell.com += cell.v * timeStep;
            cell.com.x = std::clamp(cell.com.x, -1.0, 1.0);
            cell.com.y = std::clamp(cell.com.y, -1.0, 1.0);

            // Calculate transfer amount based on 2D distance from center and dirt amount.
            // When COM is at (0,0) or cell is empty, no transfer.
            // When COM is at edge and cell is full, full transfer possible.
            double comDistance = std::sqrt(cell.com.x * cell.com.x + cell.com.y * cell.com.y);
            // Square the distance to make the effect more pronounced - max in corners, zero in center.
            double transferAmount = (comDistance * comDistance) * cell.dirty;
            
            // Calculate directional components for the transfer
            double xTransfer = (comDistance > 0.0) ? std::abs(cell.com.x / comDistance) * transferAmount : 0.0;
            double yTransfer = (comDistance > 0.0) ? std::abs(cell.com.y / comDistance) * transferAmount : 0.0;

            // Only queue moves if there's dirt to transfer and COM is off-center.
            if (cell.dirty > 0.0 && (xTransfer > 0.0 || yTransfer > 0.0)) {
                // Calculate target cell based on COM direction.
                int targetX = x;
                int targetY = y;
                Vector2d comOffset;

                if (xTransfer > 0.0) {
                    targetX += (cell.com.x > 0 ? 1 : -1);
                    // When moving horizontally, place dirt in center of target cell
                    comOffset.x = (cell.com.x > 0 ? -1.0 : 1.0);
                }
                if (yTransfer > 0.0) {
                    targetY += (cell.com.y > 0 ? 1 : -1);
                    // When moving vertically, place dirt in center of target cell
                    comOffset.y = (cell.com.y > 0 ? -1.0 : 1.0);
                }

                // Only queue move if target is within bounds.
                if (targetX >= 0 && targetX < width && targetY >= 0 && targetY < height) {
                    // Use the maximum transfer amount to determine the move amount.
                    // This ensures we don't over-transfer when moving diagonally.
                    double moveAmount = std::max(xTransfer, yTransfer);
                    moves.push_back({
                        x, y,
                        static_cast<uint32_t>(targetX),
                        static_cast<uint32_t>(targetY),
                        cell.dirty * moveAmount,
                        0.0,
                        comOffset
                    });
                }
            }
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

        // Only move if target cell isn't full.
        if (targetCell.dirty < 1.0) {
            const double moveAmount = std::min(move.amount, 1.0 - targetCell.dirty);
           
            // Calculate the fraction of dirt being moved.
            const double moveFraction = moveAmount / sourceCell.dirty;
            
            // Calculate the new COM for target cell after the move.
            Vector2d newTargetCOM = targetCell.com + move.comOffset * moveFraction;
            
            // Calculate the maximum allowed move fraction to keep COM within bounds.
            double maxMoveFraction = moveFraction;
            
            // Check each component of the COM and adjust if needed.
            if (std::abs(newTargetCOM.x) > 1.0) {
                double xFraction = (1.0 - std::abs(targetCell.com.x)) / std::abs(move.comOffset.x);
                maxMoveFraction = std::min(maxMoveFraction, xFraction);
            }
            if (std::abs(newTargetCOM.y) > 1.0) {
                double yFraction = (1.0 - std::abs(targetCell.com.y)) / std::abs(move.comOffset.y);
                maxMoveFraction = std::min(maxMoveFraction, yFraction);
            }
            
            // Use the limited move fraction.
            const double actualMoveAmount = moveAmount * (maxMoveFraction / moveFraction);
            const double actualMoveFraction = actualMoveAmount / sourceCell.dirty;
            
            // Update source cell.
            sourceCell.dirty -= actualMoveAmount;
            
            // Update target cell.
            targetCell.dirty += actualMoveAmount;
            
            // Transfer COM and velocity proportionally to the actual amount of dirt moved.
            // When dirt is added to target cell, add it at the appropriate position based on direction.
            // Calculate weighted average of COM based on mass.
            double targetMass = targetCell.dirty - actualMoveAmount;  // Original mass.
            double newMass = targetCell.dirty;  // New total mass.
            
            // Weighted average: (old_mass * old_com + new_mass * new_com) / total_mass.
            // New dirt is added at a position based on the direction of movement.
            Vector2d newDirtCOM;
            if (move.fromY < move.toY) {
                // Moving downward, add dirt at top of cell
                newDirtCOM = Vector2d(0.0, -1.0);
            } else if (move.fromY > move.toY) {
                // Moving upward, add dirt at bottom of cell
                newDirtCOM = Vector2d(0.0, 1.0);
            } else {
                // Horizontal movement, add at center
                newDirtCOM = Vector2d(0.0, 0.0);
            }
            targetCell.com = (targetCell.com * targetMass + newDirtCOM * actualMoveAmount) / newMass;
    
            // Transfer velocity with proper scaling
            targetCell.v += sourceCell.v * actualMoveFraction;
            
            // Adjust source cell's COM and velocity to account for the transferred mass.
            sourceCell.com *= (1.0 - actualMoveFraction);
            sourceCell.v *= (1.0 - actualMoveFraction);
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
        at(x, 0).dirty = 1.0;
        at(x, height - 1).dirty = 1.0;
    }
    for (uint32_t y = 0; y < height; y++) {
        at(0, y).dirty = 1.0;
        at(width - 1, y).dirty = 1.0;
    }
}

void World::reset()
{
    cells.clear();
    cells.resize(width * height);

    makeWalls();
}
