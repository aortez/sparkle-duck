#include "World.h"

#include <algorithm>
#include <iostream>
#include <random>
#include <stdexcept>

// #define LOG_DEBUG
#ifdef LOG_DEBUG
#define LOG_DEBUG(x) std::cout << x << std::endl
#else
#define LOG_DEBUG(x) ((void)0)
#endif

// Debug logging specifically for particle events
#define LOG_PARTICLES(x) std::cout << "[Particles] " << x << std::endl

namespace {
// Track the next expected time for each event
struct EventState {
    double nextTopDrop = 0.33;      // First top drop at 0.33s
    double nextInitialThrow = 0.17; // First throw at 0.17s
    double nextPeriodicThrow = 0.83; // First periodic throw at 0.83s
    bool initialThrowDone = false;  // Track if initial throw has happened
    bool topDropDone = false;       // Track if top drop has happened
};

void addParticles(World& world, uint32_t timestep, double deltaTimeMs, double timescale)
{
    static double lastSimTime = 0.0;
    static EventState eventState;
    
    // Convert deltaTimeMs to seconds and apply timescale
    const double deltaTime = (deltaTimeMs / 1000.0) * timescale;
    const double simTime = lastSimTime + deltaTime;

    LOG_PARTICLES("Timestep " << timestep << ": simTime=" << simTime 
                 << ", lastSimTime=" << lastSimTime 
                 << ", deltaTime=" << deltaTime 
                 << ", timescale=" << timescale);

    // Drop a dirt from the top
    if (!eventState.topDropDone && simTime >= eventState.nextTopDrop) {
        LOG_PARTICLES("Adding top drop at time " << simTime);
        uint32_t centerX = world.getWidth() / 2;
        Cell& cell = world.at(centerX, 1); // 1 to be just below the top wall.
        cell.dirty = 1.0;
        cell.v = Vector2d(0.0, 0.0);
        cell.com = Vector2d(0.0, 0.0);
        eventState.topDropDone = true;
    }

    // Initial throw from left center
    if (!eventState.initialThrowDone && simTime >= eventState.nextInitialThrow) {
        LOG_PARTICLES("Adding initial throw at time " << simTime);
        uint32_t centerY = world.getHeight() / 2;
        Cell& cell = world.at(2, centerY); // Against the left wall.
        cell.dirty = 1.0;
        cell.v = Vector2d(5, -5);
        cell.com = Vector2d(0.0, 0.0);
        eventState.initialThrowDone = true;
    }

    // Recurring throws every ~0.83 seconds
    const double period = 0.83;
    if (simTime >= eventState.nextPeriodicThrow) {
        LOG_PARTICLES("Adding periodic throw at time " << simTime);
        uint32_t centerY = world.getHeight() / 2;
        Cell& cell = world.at(2, centerY); // Against the left wall.
        cell.dirty = 1.0;
        cell.v = Vector2d(5, -5);
        cell.com = Vector2d(0.0, 0.0);
        // Schedule next throw
        eventState.nextPeriodicThrow += period;
    }

    lastSimTime = simTime;
}
} // namespace

World::World(uint32_t width, uint32_t height, lv_obj_t* draw_area)
    : width(width), height(height), cells(width * height), draw_area(draw_area)
{}

void World::advanceTime(uint32_t deltaTimeMs)
{
    if (addParticlesEnabled) {
        addParticles(*this, timestep++, deltaTimeMs, timescale);
    }
    else {
        timestep++;
    }

    // Use the member gravity variable
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
            // Skip the cell being dragged
            if (isDragging && static_cast<int>(x) == lastDragCellX
                && static_cast<int>(y) == lastDragCellY) {
                continue;
            }
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
            const Vector2d predictedCom = cell.com + cell.v * timeStep;

            // Debug output for transfer logic.
            LOG_DEBUG(
                "Cell (" << x << "," << y << "): predictedCom=(" << predictedCom.x << ","
                         << predictedCom.y << "), v=(" << cell.v.x << "," << cell.v.y << "), com=(" << cell.com.x << "," << cell.com.y << "), dirty=" << cell.dirty);

            // Calculate if predicted COM will be in neighboring cell space or moving toward it.
            bool shouldTransferX = false;
            bool shouldTransferY = false;
            int targetX = x;
            int targetY = y;
            Vector2d comOffset;

            // Check horizontal transfer.
            if (predictedCom.x > 1.0) {
                shouldTransferX = true;
                targetX = x + 1;
                comOffset.x = predictedCom.x - 2.0;
                LOG_DEBUG(
                    "  Transfer right: predictedCom.x=" << predictedCom.x << ", v.x=" << cell.v.x
                                                        << ", com.x=" << cell.com.x);
            }
            else if (predictedCom.x < -1.0) {
                shouldTransferX = true;
                targetX = x - 1;
                comOffset.x = predictedCom.x + 2.0;
                LOG_DEBUG(
                    "  Transfer left: predictedCom.x=" << predictedCom.x << ", v.x=" << cell.v.x
                                                       << ", com.x=" << cell.com.x);
            }

            // Check vertical transfer.
            if (predictedCom.y > 1.0) {
                shouldTransferY = true;
                targetY = y + 1;
                comOffset.y = predictedCom.y - 2.0;
                LOG_DEBUG(
                    "  Transfer down: predictedCom.y=" << predictedCom.y << ", v.y=" << cell.v.y
                                                    << ", com.y=" << cell.com.y);
            }
            else if (predictedCom.y < -1.0) {
                shouldTransferY = true;
                targetY = y - 1;
                comOffset.y = predictedCom.y + 2.0;
                LOG_DEBUG(
                    "  Transfer up: predictedCom.y=" << predictedCom.y << ", v.y=" << cell.v.y
                                                    << ", com.y=" << cell.com.y);
            }

            // If no transfer needed in a direction, preserve that component of the COM
            if (!shouldTransferX) {
                comOffset.x = predictedCom.x;
            }
            if (!shouldTransferY) {
                comOffset.y = predictedCom.y;
            }

            // Only queue moves if the predicted COM will be in neighboring cell space.
            // Otherwise go ahead and execute local moves immediately.
            if (!shouldTransferX && !shouldTransferY) {
                // Move COM internally.
                cell.com = predictedCom;
                continue;
            }
            
            // Queue moves for transfer.
            // Only queue move if target is within bounds.
            // TODO: Eventually we will want to implement reflections, rather than skipping the move.
            if (targetX >= 0 && targetX < width && targetY >= 0 && targetY < height) {
                Cell& targetCell = at(targetX, targetY);
                // Calculate transfer amount based on available dirt and space.
                const double moveAmount = std::min({
                    cell.dirty,                  // Can't move more than exists.
                    1 - targetCell.dirty               // Can't exceed target cell's capacity.
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
                    LOG_DEBUG(
                        "  Queued move: from=(" << x << "," << y << "), to=(" << targetX << ","
                                                << targetY << "), amount=" << moveAmount);
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

        // Calculate maximum possible transfer while respecting capacity
        const double availableSpace = 1.0 - targetCell.dirty;
        const double moveAmount = std::min({
            move.amount,
            sourceCell.dirty, // Can't move more than we have.
            availableSpace    // Can't exceed target capacity.
        });

        if (moveAmount <= 0.0) {
            continue;
        }

        // Calculate the fraction being moved.
        const double moveFraction = moveAmount / sourceCell.dirty;

        // Update source cell
        sourceCell.dirty -= moveAmount;

        // Update target cell
        const double oldTargetMass = targetCell.dirty;
        targetCell.dirty += moveAmount;

        // Calculate the expected COM position in the target cell based on the actual movement
        // The COM offset represents how far the dirt moved during this timestep
        Vector2d expectedCom = move.comOffset;

        // Update target cell's COM using weighted average
        if (oldTargetMass == 0.0) {
            // First dirt in cell, use the expected COM directly
            targetCell.com = expectedCom;
        }
        else {
            // Weighted average of existing COM and the new COM based on mass.
            // This preserves both mass and energy in the system.
            targetCell.com =
                (targetCell.com * oldTargetMass + expectedCom * moveAmount) / targetCell.dirty;
        }

        // Transfer momentum.
        if (targetCell.dirty > 0.0) {
            targetCell.v =
                (targetCell.v * oldTargetMass + sourceCell.v * moveAmount) / targetCell.dirty;
        }

        // Update source cell's COM and velocity if any dirt remains.
        if (sourceCell.dirty > 0.0) {
            sourceCell.v *= (1.0 - moveFraction);
            // Adjust source COM to account for removed mass.
            sourceCell.com = sourceCell.com * (1.0 - moveFraction);
        }
        else {
            sourceCell.v = Vector2d(0.0, 0.0);
            sourceCell.com = Vector2d(0.0, 0.0);
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
        std::cout << "World::at: Attempted to access coordinates (" << x << "," << y
                  << ") but world size is " << width << "x" << height << std::endl;
        throw std::out_of_range("World::at: Coordinates out of range");
    }
    return cells[coordToIndex(x, y)];
}

const Cell& World::at(uint32_t x, uint32_t y) const
{
    if (x >= width || y >= height) {
        std::cout << "World::at: Attempted to access coordinates (" << x << "," << y
                  << ") but world size is " << width << "x" << height << std::endl;
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
    // makeWalls();
}

void World::addDirtAtPixel(int pixelX, int pixelY)
{
    // Convert pixel coordinates to cell coordinates
    int cellX = pixelX / Cell::WIDTH;
    int cellY = pixelY / Cell::HEIGHT;

    // Check if coordinates are within bounds
    if (cellX >= 0 && cellX < static_cast<int>(width) && cellY >= 0
        && cellY < static_cast<int>(height)) {
        Cell& cell = at(cellX, cellY);
        cell.dirty = 1.0;
        cell.v = Vector2d(0.0, 0.0);
        cell.com = Vector2d(0.0, 0.0);
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
        cell.dirty = lastCellOriginalDirt;
    }
    lastDragCellX = -1;
    lastDragCellY = -1;
    lastCellOriginalDirt = 0.0;
}

void World::startDragging(int pixelX, int pixelY)
{
    int cellX, cellY;
    pixelToCell(pixelX, pixelY, cellX, cellY);

    // Check if coordinates are within bounds
    if (cellX >= 0 && cellX < static_cast<int>(width) && cellY >= 0
        && cellY < static_cast<int>(height)) {
        Cell& cell = at(cellX, cellY);

        // Only start dragging if there's dirt to drag
        if (cell.dirty > MIN_DIRT_THRESHOLD) {
            isDragging = true;
            dragStartX = cellX;
            dragStartY = cellY;
            draggedDirt = cell.dirty;
            draggedVelocity = cell.v;

            // Clear the source cell
            cell.dirty = 0.0;
            cell.v = Vector2d(0.0, 0.0);
            cell.com = Vector2d(0.0, 0.0);

            // Visual feedback: fill the cell under the cursor
            restoreLastDragCell();
            lastDragCellX = cellX;
            lastDragCellY = cellY;
            lastCellOriginalDirt = 0.0; // Already set to 0 above
            cell.dirty = draggedDirt;

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

        // Visual feedback: restore previous cell, fill new cell
        if (cellX != lastDragCellX || cellY != lastDragCellY) {
            restoreLastDragCell();
            Cell& cell = at(cellX, cellY);
            lastDragCellX = cellX;
            lastDragCellY = cellY;
            lastCellOriginalDirt = cell.dirty;
            cell.dirty = draggedDirt;
        }

        // Track recent positions
        recentPositions.push_back({ cellX, cellY });
        if (recentPositions.size() > MAX_RECENT_POSITIONS) {
            recentPositions.erase(recentPositions.begin());
        }

        // Debug: Print current drag state
        std::cout << "Drag Update - Cell: (" << cellX << "," << cellY
                  << ") Recent positions: " << recentPositions.size() << " Current velocity: ("
                  << draggedVelocity.x << "," << draggedVelocity.y << ")\n";
    }
}

void World::endDragging(int pixelX, int pixelY)
{
    if (!isDragging) return;

    int cellX, cellY;
    pixelToCell(pixelX, pixelY, cellX, cellY);

    // Debug: Print recent positions before calculating velocity
    std::cout << "\nRelease Debug:\n";
    std::cout << "Recent positions: ";
    for (const auto& pos : recentPositions) {
        std::cout << "(" << pos.first << "," << pos.second << ") ";
    }
    std::cout << "\n";

    // Restore the last cell before finalizing
    restoreLastDragCell();

    // Check if coordinates are within bounds
    if (cellX >= 0 && cellX < static_cast<int>(width) && cellY >= 0
        && cellY < static_cast<int>(height)) {
        Cell& cell = at(cellX, cellY);

        // Calculate average velocity from recent positions
        if (recentPositions.size() > 1) {
            double avgDx = 0.0, avgDy = 0.0;
            for (size_t i = 1; i < recentPositions.size(); ++i) {
                double dx = recentPositions[i].first - recentPositions[i - 1].first;
                double dy = recentPositions[i].second - recentPositions[i - 1].second;
                avgDx += dx;
                avgDy += dy;
                std::cout << "Step " << i << " delta: (" << dx << "," << dy << ")\n";
            }
            avgDx /= (recentPositions.size() - 1);
            avgDy /= (recentPositions.size() - 1);
            draggedVelocity = Vector2d(avgDx * 2.0, avgDy * 2.0); // Scale factor for better feel

            std::cout << "Final velocity: (" << draggedVelocity.x << "," << draggedVelocity.y
                      << ")\n";
        }
        else {
            std::cout << "Not enough positions for velocity calculation\n";
        }

        // Place the dirt in the target cell
        cell.dirty = draggedDirt;
        cell.v = draggedVelocity;
        cell.com = Vector2d(0.0, 0.0);

        std::cout << "Placed dirt at (" << cellX << "," << cellY << ") with velocity (" << cell.v.x
                  << "," << cell.v.y << ")\n";
    }

    // Reset drag state
    isDragging = false;
    draggedDirt = 0.0;
    draggedVelocity = Vector2d(0.0, 0.0);
    recentPositions.clear();
}
