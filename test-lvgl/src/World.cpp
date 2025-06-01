#include "World.h"
#include "Cell.h"
#include "Timers.h"

#include <algorithm>
#include <iostream>
#include <random>
#include <stdexcept>

// #define LOG_DEBUG
#ifdef LOG_DEBU
#define LOG_DEBUG(x) std::cout << x << std::endl
#else
#define LOG_DEBUG(x) ((void)0)
#endif

// Debug logging specifically for particle events
// #define LOG_PARTICLES
#ifdef LOG_PARTICLES
#define LOG_PARTICLES(x) std::cout << "[Particles] " << x << std::endl
#else
#define LOG_PARTICLES(x) ((void)0)
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

void addParticles(World& world, uint32_t timestep, double deltaTimeMs, double timescale)
{
    static double lastSimTime = 0.0;
    static EventState eventState;

    // Convert deltaTimeMs to seconds and apply timescale
    const double deltaTime = (deltaTimeMs / 1000.0) * timescale;
    const double simTime = lastSimTime + deltaTime;

    LOG_PARTICLES(
        "Timestep " << timestep << ": simTime=" << simTime << ", lastSimTime=" << lastSimTime
                    << ", deltaTime=" << deltaTime << ", timescale=" << timescale);

    // Constants for sweep behavior
    const double SWEEP_PERIOD = 2.0;  // Time for one complete sweep (left to right and back)
    const double DIRT_PERIOD = 0.5;   // Period for dirt amount oscillation
    const double SWEEP_SPEED = 2.0;   // Speed of the sweep
    const double DIRT_AMPLITUDE = 0.5; // Amplitude of dirt oscillation (0.5 means dirt varies from 0.5 to 1.0)
    const double BEAT_PERIOD = 0.5;   // Length of one beat in seconds
    const int BEATS_PER_PATTERN = 8;  // Total beats in the pattern
    const int BEATS_ON = 2;           // Number of beats the emitter is on

    // Update beat time
    eventState.beatTime += deltaTime;
    if (eventState.beatTime >= BEAT_PERIOD * BEATS_PER_PATTERN) {
        eventState.beatTime -= BEAT_PERIOD * BEATS_PER_PATTERN;
    }

    // Calculate current beat in the pattern (0 to BEATS_PER_PATTERN-1)
    int currentBeat = static_cast<int>(eventState.beatTime / BEAT_PERIOD);
    bool isEmitterOn = currentBeat < BEATS_ON;

    // Only update sweep and emit particles if the emitter is on
    if (isEmitterOn) {
        // Update sweep time
        eventState.sweepTime += deltaTime;

        // Calculate sweep position (x coordinate)
        double sweepPhase = (eventState.sweepTime / SWEEP_PERIOD) * 2.0 * M_PI;
        double sweepX = (std::sin(sweepPhase) + 1.0) * 0.5; // Maps to [0,1]
        uint32_t xPos = static_cast<uint32_t>(sweepX * (world.getWidth() - 2)) + 1; // Maps to [1,width-2]

        // Calculate dirt amount oscillation
        double dirtPhase = (eventState.sweepTime / DIRT_PERIOD) * 2.0 * M_PI;
        double dirtAmount = 0.5 + DIRT_AMPLITUDE * std::sin(dirtPhase); // Oscillates between 0.5 and 1.0

        // Emit particle at current sweep position
        Cell& cell = world.at(xPos, 1); // 1 to be just below the top wall
        cell.update(dirtAmount, Vector2d(0.0, 0.0), Vector2d(0.0, 0.0));
        LOG_PARTICLES("Sweep emitter at x=" << xPos << " with dirt=" << dirtAmount << " (beat " << currentBeat << ")");
    }

    // Drop a dirt from the top
    if (!eventState.topDropDone && simTime >= eventState.nextTopDrop) {
        LOG_PARTICLES("Adding top drop at time " << simTime);
        uint32_t centerX = world.getWidth() / 2;
        Cell& cell = world.at(centerX, 1); // 1 to be just below the top wall.
        cell.update(1.0, Vector2d(0.0, 0.0), Vector2d(0.0, 0.0));
        eventState.topDropDone = true;
    }

    // Initial throw from left center
    if (!eventState.initialThrowDone && simTime >= eventState.nextInitialThrow) {
        LOG_PARTICLES("Adding initial throw at time " << simTime);
        uint32_t centerY = world.getHeight() / 2;
        Cell& cell = world.at(2, centerY); // Against the left wall.
        cell.update(1.0, Vector2d(0.0, 0.0), Vector2d(5, -5));
        eventState.initialThrowDone = true;
    }

    // Recurring throws every ~0.83 seconds
    const double period = 0.83;
    if (simTime >= eventState.nextPeriodicThrow) {
        LOG_PARTICLES("Adding periodic throw at time " << simTime);
        uint32_t centerY = world.getHeight() / 2;
        Cell& cell = world.at(2, centerY); // Against the left wall.
        cell.update(1.0, Vector2d(0.0, 0.0), Vector2d(10, -10));
        // Schedule next throw
        eventState.nextPeriodicThrow += period;
    }

    // Recurring throws from right side every ~0.83 seconds
    if (simTime >= eventState.nextRightThrow) {
        LOG_PARTICLES("Adding right periodic throw at time " << simTime);
        uint32_t centerY = world.getHeight() / 2;
        Cell& cell = world.at(world.getWidth() - 3, centerY); // Against the right wall.
        cell.update(1.0, Vector2d(0.0, 0.0), Vector2d(-10, -10));
        // Schedule next throw
        eventState.nextRightThrow += period;
    }

    lastSimTime = simTime;
}
} // namespace

// Initialize static member variables
double World::ELASTICITY_FACTOR = 0.8;  // Energy preserved in reflections (0.0 to 1.0)

World::World(uint32_t width, uint32_t height, lv_obj_t* draw_area)
    : width(width), height(height), cells(width * height), draw_area(draw_area)
{
    // Initialize timers
    timers.startTimer("total_simulation");
}

World::~World()
{
    // Stop the total simulation timer and dump stats
    timers.stopTimer("total_simulation");
    dumpTimerStats();
}

void World::advanceTime(uint32_t deltaTimeMs)
{
    timers.startTimer("advance_time");

    if (addParticlesEnabled) {
        timers.startTimer("add_particles");
        addParticles(*this, timestep++, deltaTimeMs, timescale);
        timers.stopTimer("add_particles");
    }
    else {
        timestep++;
    }

    // Use the member gravity variable
    const double timeStep =
        deltaTimeMs / 1000.0 * timescale; // Convert to seconds and apply timescale.

    // Process any pending drag end
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

    struct DirtMove {
        uint32_t fromX;
        uint32_t fromY;
        uint32_t toX;
        uint32_t toY;
        double dirtAmount;    // Amount of dirt to move
        double waterAmount;   // Amount of water to move
        double actualDirt;    // Actual amount of dirt moved after capacity checks
        double actualWater;   // Actual amount of water moved after capacity checks
        Vector2d comOffset;   // COM offset to apply to the target cell.
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

            // Skip empty cells or cells with mass below threshold.
            if (cell.percentFull() < MIN_DIRT_THRESHOLD) {
                // Track the mass being removed.
                removedMass += cell.percentFull();
                // Reset them to initial conditions.
                cell.update(0.0, Vector2d(0.0, 0.0), Vector2d(0.0, 0.0));
                cell.water = 0.0;
                continue;
            }

            // Debug: Log initial state
            if (cell.v.x != 0.0 || cell.v.y != 0.0) {
                LOG_DEBUG(
                    "Cell (" << x << "," << y << ") initial state: v=(" << cell.v.x << ","
                             << cell.v.y << "), com=(" << cell.com.x << "," << cell.com.y << ")");
            }

            // Apply gravity.
            cell.v.y += gravity * timeStep;

            // Apply water cohesion and viscosity if this is a water cell
            if (cell.water >= MIN_DIRT_THRESHOLD) {
                // Check all 8 neighboring cells for cohesion
                for (int dy = -1; dy <= 1; dy++) {
                    for (int dx = -1; dx <= 1; dx++) {
                        if (dx == 0 && dy == 0) continue;
                        
                        int nx = x + dx;
                        int ny = y + dy;
                        if (nx >= 0 && nx < width && ny >= 0 && ny < height) {
                            Cell& neighbor = at(nx, ny);
                            
                            // Apply cohesion force
                            Vector2d cohesion = cell.calculateWaterCohesion(cell, neighbor);
                            cell.v += cohesion * timeStep;
                            
                            // Apply viscosity
                            cell.applyViscosity(neighbor);
                        }
                    }
                }
            }

            // Apply cursor force if active
            if (cursorForceEnabled && cursorForceActive) {
                double dx = cursorForceX - static_cast<int>(x);
                double dy = cursorForceY - static_cast<int>(y);
                double distance = std::sqrt(dx * dx + dy * dy);

                if (distance <= CURSOR_FORCE_RADIUS) {
                    // Calculate force based on distance (stronger when closer)
                    double forceFactor =
                        (1.0 - distance / CURSOR_FORCE_RADIUS) * CURSOR_FORCE_STRENGTH;
                    // Normalize direction and apply force
                    if (distance > 0) {
                        cell.v.x += (dx / distance) * forceFactor * timeStep;
                        cell.v.y += (dy / distance) * forceFactor * timeStep;
                    }
                }
            }

            // Calculate predicted COM position after this timestep.
            const Vector2d predictedCom = cell.com + cell.v * timeStep;

            // Debug output for transfer logic.
            LOG_DEBUG(
                "Cell (" << x << "," << y << "): predictedCom=(" << predictedCom.x << ","
                         << predictedCom.y << "), v=(" << cell.v.x << "," << cell.v.y << "), com=("
                         << cell.com.x << "," << cell.com.y << "), dirt=" << cell.dirt
                         << ", water=" << cell.water);

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
                comOffset.x = std::min(predictedCom.x - 2.0, 1.0);
                LOG_DEBUG(
                    "  Transfer right: predictedCom.x=" << predictedCom.x << ", v.x=" << cell.v.x
                                                        << ", com.x=" << cell.com.x);
            }
            else if (predictedCom.x < -1.0) {
                shouldTransferX = true;
                targetX = x - 1;
                comOffset.x = std::max(predictedCom.x + 2.0, -1.0);
                LOG_DEBUG(
                    "  Transfer left: predictedCom.x=" << predictedCom.x << ", v.x=" << cell.v.x
                                                       << ", com.x=" << cell.com.x);
            }

            // Check vertical transfer.
            if (predictedCom.y > 1.0) {
                shouldTransferY = true;
                targetY = y + 1;
                comOffset.y = std::min(predictedCom.y - 2.0, 1.0);
                LOG_DEBUG(
                    "  Transfer down: predictedCom.y=" << predictedCom.y << ", v.y=" << cell.v.y
                                                       << ", com.y=" << cell.com.y);
            }
            else if (predictedCom.y < -1.0) {
                shouldTransferY = true;
                targetY = y - 1;
                comOffset.y = std::max(predictedCom.y + 2.0, -1.0);
                LOG_DEBUG(
                    "  Transfer up: predictedCom.y=" << predictedCom.y << ", v.y=" << cell.v.y
                                                     << ", com.y=" << cell.com.y);
            }

            // Only queue moves if the predicted COM will be in neighboring cell space.
            // Otherwise go ahead and execute local moves immediately.
            if (!shouldTransferX && !shouldTransferY) {
                // Move COM internally.
                cell.update(cell.dirt, predictedCom, cell.v);
                continue;
            }

            // Queue moves for transfer.
            // Only queue move if target is within bounds.
            if (targetX >= 0 && targetX < width && targetY >= 0 && targetY < height) {
                Cell& targetCell = at(targetX, targetY);

                // Calculate transfer amounts based on available space in target cell
                const double availableSpace = 1.0 - targetCell.percentFull();
                const double totalMass = cell.dirt + cell.water;
                
                // Calculate fragmentation based on velocity
                const double velocityMagnitude = cell.v.mag();
                const double FRAGMENTATION_FACTOR = 0.01; // Reduced from 0.05 to minimize fragmentation
                const double fragmentation = std::min(0.1, velocityMagnitude * FRAGMENTATION_FACTOR); // Reduced max from 0.2 to 0.1
                
                // Calculate how much mass will actually move
                const double moveAmount = std::min({
                    totalMass * (1.0 - fragmentation),  // Leave some behind based on velocity
                    availableSpace      // Can't exceed target cell's capacity.
                });

                // Calculate proportions of dirt and water to move
                double dirtProportion = cell.dirt / totalMass;
                double waterProportion = cell.water / totalMass;
                double dirtAmount = moveAmount * dirtProportion;
                double waterAmount = moveAmount * waterProportion;

                moves.push_back({ x,
                                    y,
                                    static_cast<uint32_t>(targetX),
                                    static_cast<uint32_t>(targetY),
                                    dirtAmount,
                                    waterAmount,
                                    0.0,
                                    0.0,
                                    comOffset });
                LOG_DEBUG(
                    "  Queued move: from=(" << x << "," << y << ") to=(" << targetX << ","
                                            << targetY << "), dirt=" << dirtAmount 
                                            << ", water=" << waterAmount
                                            << ", fragmentation=" << fragmentation);
            }
        }
    }

    // Shuffle moves to avoid bias (which ones are applied or not).
    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(moves.begin(), moves.end(), g);

    // Second pass: apply valid moves.
    for (const auto& move : moves) {
        Cell& sourceCell = at(move.fromX, move.fromY);
        Cell& targetCell = at(move.toX, move.toY);

        // Debug: Log transfer state
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
            sourceCell.percentFull(), // Can't move more than we have.
            availableSpace    // Can't exceed target capacity.
        });

        if (moveAmount <= 0.0) {
            // Instead of skipping, handle internal reflection
            // Calculate the normal vector for reflection
            Vector2d normal = Vector2d((int)move.toX - (int)move.fromX, (int)move.toY - (int)move.fromY).normalize();
            
            // Project velocity onto normal and reflect it
            double vn = sourceCell.v.dot(normal);
            Vector2d vt = sourceCell.v - normal * vn;
            
            // Reflect with elasticity
            double elasticity = World::ELASTICITY_FACTOR;
            double vn_after = -vn * elasticity;
            
            // Update velocity with reflected component
            sourceCell.v = vt + normal * vn_after;
            
            // Update COM to stay within cell bounds
            Vector2d newCom = sourceCell.com;
            if (normal.x != 0.0) {
                newCom.x = std::clamp(newCom.x, -1.0, 1.0);
            }
            if (normal.y != 0.0) {
                newCom.y = std::clamp(newCom.y, -1.0, 1.0);
            }
            
            // Update the cell with new COM and reflected velocity
            sourceCell.update(sourceCell.dirt, newCom, sourceCell.v);
            continue;
        }

        // --- ELASTIC COLLISION RESPONSE (only if both cells have mass) ---
        if (sourceCell.percentFull() > MIN_DIRT_THRESHOLD && targetCell.percentFull() > MIN_DIRT_THRESHOLD) {
            // Normal vector from source to target
            Vector2d normal = Vector2d((int)move.toX - (int)move.fromX, (int)move.toY - (int)move.fromY).normalize();
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
        const double actualDirt = moveAmount * dirtProportion;
        const double actualWater = moveAmount * waterProportion;

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
            Vector2d newCom =
                (targetCell.com * oldTargetMass + expectedCom * moveAmount) / (oldTargetMass + moveAmount);
            targetCell.update(targetCell.dirt + actualDirt, newCom, targetCell.v);
            targetCell.water += actualWater;
        }

        // Update source cell's COM and velocity if any mass remains.
        if (sourceCell.percentFull() > 0.0) {
            // Preserve velocity if no collision occurred
            Vector2d newV = sourceCell.v;
            // Scale COM based on remaining mass
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

    // Update total mass after all moves are complete.
    totalMass = 0.0;
    for (uint32_t y = 0; y < height; y++) {
        for (uint32_t x = 0; x < width; x++) {
            totalMass += at(x, y).percentFull();
        }
    }

    timers.stopTimer("advance_time");
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

void World::fillWithDirt()
{
    for (uint32_t y = 0; y < height; y++) {
        for (uint32_t x = 0; x < width; x++) {
            at(x, y).update(0.5, Vector2d(0.0, 0.0), Vector2d(1, 0.0));
            at(x, y).markDirty();
        }
    }
}

void World::makeWalls()
{
    for (uint32_t x = 0; x < width; x++) {
        //        at(x, 0).dirty = 1.0;
        at(x, height - 1).update(1.0, Vector2d(0.0, 0.0), Vector2d(0.0, 0.0));
        at(x, height - 1).markDirty();
    }
    for (uint32_t y = 0; y < height; y++) {
        at(0, y).update(1.0, Vector2d(0.0, 0.0), Vector2d(0.0, 0.0));
        at(0, y).markDirty();
        at(width - 1, y).update(1.0, Vector2d(0.0, 0.0), Vector2d(0.0, 0.0));
        at(width - 1, y).markDirty();
    }
}

void World::reset()
{
    timestep = 0;
    cells.clear();
    cells.resize(width * height);
    makeWalls();
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
        cell.update(1.0, Vector2d(0.0, 0.0), Vector2d(0.0, 0.0));
        cell.markDirty();
    }
}

void World::addWaterAtPixel(int pixelX, int pixelY)
{
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

void World::dumpTimerStats() const
{
    std::cout << "\nTimer Statistics:" << std::endl;
    std::cout << "----------------" << std::endl;

    // Get total simulation time
    double totalTime = timers.getAccumulatedTime("total_simulation");
    uint32_t totalCalls = timers.getCallCount("total_simulation");
    std::cout << "Total Simulation Time: " << totalTime << "ms ("
              << (totalCalls > 0 ? totalTime / totalCalls : 0) << "ms avg per call, " << totalCalls
              << " calls)" << std::endl;

    // Get advance time stats
    double advanceTime = timers.getAccumulatedTime("advance_time");
    uint32_t advanceCalls = timers.getCallCount("advance_time");
    std::cout << "Physics Update Time: " << advanceTime << "ms ("
              << (advanceTime / totalTime * 100.0) << "% of total, "
              << (advanceCalls > 0 ? advanceTime / advanceCalls : 0) << "ms avg per call, "
              << advanceCalls << " calls)" << std::endl;

    // Get draw time stats
    double drawTime = timers.getAccumulatedTime("draw");
    uint32_t drawCalls = timers.getCallCount("draw");
    std::cout << "Drawing Time: " << drawTime << "ms (" << (drawTime / totalTime * 100.0)
              << "% of total, " << (drawCalls > 0 ? drawTime / drawCalls : 0) << "ms avg per call, "
              << drawCalls << " calls)" << std::endl;

    // Get particle addition time if enabled
    if (addParticlesEnabled) {
        double particleTime = timers.getAccumulatedTime("add_particles");
        uint32_t particleCalls = timers.getCallCount("add_particles");
        std::cout << "Particle Addition Time: " << particleTime << "ms ("
                  << (particleTime / totalTime * 100.0) << "% of total, "
                  << (particleCalls > 0 ? particleTime / particleCalls : 0) << "ms avg per call, "
                  << particleCalls << " calls)" << std::endl;
    }

    // Get drag processing time
    double dragTime = timers.getAccumulatedTime("process_drag_end");
    uint32_t dragCalls = timers.getCallCount("process_drag_end");
    std::cout << "Drag Processing Time: " << dragTime << "ms (" << (dragTime / totalTime * 100.0)
              << "% of total, " << (dragCalls > 0 ? dragTime / dragCalls : 0) << "ms avg per call, "
              << dragCalls << " calls)" << std::endl;

    std::cout << "----------------" << std::endl;
}
