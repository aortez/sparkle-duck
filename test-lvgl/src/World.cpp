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
    const double timeStep = deltaTimeMs / 1000.0; // Convert to seconds.

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

            // Apply gravity.
            cell.v.y += gravity * timeStep;

            // Move internally.
            cell.com += cell.v * timeStep;

            // Check if center of mass is outside cell bounds.
            if (cell.com.x < -1.0 || cell.com.x > 1.0 || cell.com.y < -1.0 || cell.com.y > 1.0) {
                // Determine target cell based on com direction.
                int targetX = x;
                int targetY = y;
                Vector2d comOffset;
                
                if (cell.com.x < -1.0) {
                    targetX--;
                    comOffset.x = 2.0;  // Move COM right by 2 units (cell width).
                }
                else if (cell.com.x > 1.0) {
                    targetX++;
                    comOffset.x = -2.0; // Move COM left by 2 units.
                }
                
                if (cell.com.y < -1.0) {
                    targetY--;
                    comOffset.y = 2.0;  // Move COM down by 2 units (cell height).
                }
                else if (cell.com.y > 1.0) {
                    targetY++;
                    comOffset.y = -2.0; // Move COM up by 2 units.
                }

                // Only queue move if target is within bounds.
                if (targetX >= 0 && targetX < width && targetY >= 0 && targetY < height) {
                    moves.push_back({x, y, static_cast<uint32_t>(targetX), static_cast<uint32_t>(targetY), 
                                   cell.dirty, 0.0, comOffset});
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
            double moveAmount = std::min(move.amount, 1.0 - targetCell.dirty);
            
            // Calculate the fraction of dirt being moved.
            double fraction = moveAmount / sourceCell.dirty;
            
            // Update source cell.
            sourceCell.dirty -= moveAmount;
            
            // Update target cell.
            targetCell.dirty += moveAmount;
            
            // Adjust COM of both cells.
            // For source cell, scale down its COM by the remaining fraction.
            sourceCell.com *= (1.0 - fraction);
            
            // For target cell, blend its COM with the incoming COM.
            // The incoming COM needs to be adjusted by the offset and scaled by the fraction.
            Vector2d incomingCom = sourceCell.com + move.comOffset;
            targetCell.com = (targetCell.com * targetCell.dirty + incomingCom * moveAmount) / 
                           (targetCell.dirty + moveAmount);
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
}
