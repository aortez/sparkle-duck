#include "World.h"
#include "Cell.h"
#include "Timers.h"
#include <cassert>

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

    // Create default world setup
    worldSetup = std::make_unique<DefaultWorldSetup>();
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

    if (addParticlesEnabled) {
        timers.startTimer("add_particles");
        worldSetup->addParticles(*this, timestep++, deltaTimeSeconds);
        timers.stopTimer("add_particles");
    }
    else {
        timestep++;
    }

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
            cell.v.y += gravity * deltaTimeSeconds;

            // Apply water cohesion and viscosity if this is a water cell.
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
                            cell.v += cohesion * deltaTimeSeconds;

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
                        cell.v.x += (dx / distance) * forceFactor * deltaTimeSeconds;
                        cell.v.y += (dy / distance) * forceFactor * deltaTimeSeconds;
                    }
                }
            }

            // Calculate predicted COM position after this timestep.
            const Vector2d predictedCom = cell.com + cell.v * deltaTimeSeconds;

            // Debug output for transfer logic.
            LOG_DEBUG(
                "Cell (" << x << "," << y << "): predictedCom=(" << predictedCom.x << ","
                         << predictedCom.y << "), v=(" << cell.v.x << "," << cell.v.y << "), com=("
                         << cell.com.x << "," << cell.com.y << "), dirt=" << cell.dirt
                         << ", water=" << cell.water);

            // COM deflection threshold for triggering transfers
            static constexpr double COM_DEFLECTION_THRESHOLD = 0.6;

            // Check for transfers based on COM deflection from center
            bool shouldTransferX = false;
            bool shouldTransferY = false;
            int targetX = x;
            int targetY = y;
            Vector2d comOffset;

            // Check horizontal transfer based on COM deflection.
            if (cell.com.x > COM_DEFLECTION_THRESHOLD) {
                shouldTransferX = true;
                targetX = x + 1;
                // Calculate natural position: source COM minus 2.0 (one cell width)
                double naturalX = cell.com.x - 2.0;
                // Clamp to dead zone to prevent immediate re-transfer
                comOffset.x = std::max(
                    -COM_DEFLECTION_THRESHOLD, std::min(COM_DEFLECTION_THRESHOLD, naturalX));
                LOG_DEBUG(
                    "  Transfer right: com.x=" << cell.com.x
                                               << " > threshold=" << COM_DEFLECTION_THRESHOLD
                                               << ", v.x=" << cell.v.x << ", natural=" << naturalX
                                               << ", target_com.x=" << comOffset.x);
            }
            else if (cell.com.x < -COM_DEFLECTION_THRESHOLD) {
                shouldTransferX = true;
                targetX = x - 1;
                // Calculate natural position: source COM plus 2.0 (one cell width)
                double naturalX = cell.com.x + 2.0;
                // Clamp to dead zone to prevent immediate re-transfer
                comOffset.x = std::max(
                    -COM_DEFLECTION_THRESHOLD, std::min(COM_DEFLECTION_THRESHOLD, naturalX));
                LOG_DEBUG(
                    "  Transfer left: com.x=" << cell.com.x
                                              << " < threshold=" << -COM_DEFLECTION_THRESHOLD
                                              << ", v.x=" << cell.v.x << ", natural=" << naturalX
                                              << ", target_com.x=" << comOffset.x);
            }

            // Check vertical transfer based on COM deflection.
            if (cell.com.y > COM_DEFLECTION_THRESHOLD) {
                shouldTransferY = true;
                targetY = y + 1;
                // Calculate natural position: source COM minus 2.0 (one cell height)
                double naturalY = cell.com.y - 2.0;
                // Clamp to dead zone to prevent immediate re-transfer
                comOffset.y = std::max(
                    -COM_DEFLECTION_THRESHOLD, std::min(COM_DEFLECTION_THRESHOLD, naturalY));
                LOG_DEBUG(
                    "  Transfer down: com.y=" << cell.com.y
                                              << " > threshold=" << COM_DEFLECTION_THRESHOLD
                                              << ", v.y=" << cell.v.y << ", natural=" << naturalY
                                              << ", target_com.y=" << comOffset.y);
            }
            else if (cell.com.y < -COM_DEFLECTION_THRESHOLD) {
                shouldTransferY = true;
                targetY = y - 1;
                // Calculate natural position: source COM plus 2.0 (one cell height)
                double naturalY = cell.com.y + 2.0;
                // Clamp to dead zone to prevent immediate re-transfer
                comOffset.y = std::max(
                    -COM_DEFLECTION_THRESHOLD, std::min(COM_DEFLECTION_THRESHOLD, naturalY));
                LOG_DEBUG(
                    "  Transfer up: com.y=" << cell.com.y
                                            << " < threshold=" << -COM_DEFLECTION_THRESHOLD
                                            << ", v.y=" << cell.v.y << ", natural=" << naturalY
                                            << ", target_com.y=" << comOffset.y);
            }

            // Handle transfers - prioritize diagonal transfers when both X and Y are needed
            if (shouldTransferX || shouldTransferY) {
                // Calculate total mass and proportions
                const double totalMass = cell.dirt + cell.water;
                const double dirtProportion = cell.dirt / totalMass;
                const double waterProportion = cell.water / totalMass;

                // Track if any transfer occurred
                bool transferOccurred = false;

                // Try diagonal transfer first if both X and Y are needed
                if (shouldTransferX && shouldTransferY) {
                    int checkX = targetX;
                    int checkY = targetY;
                    bool xInBounds = (checkX >= 0 && checkX < width);
                    bool yInBounds = (checkY >= 0 && checkY < height);

                    if (xInBounds && yInBounds) {
                        Cell& targetCell = at(checkX, checkY);
                        if (targetCell.percentFull() < 1.0) {
                            // Calculate transfer amounts for diagonal direction
                            const double availableSpace = 1.0 - targetCell.percentFull();
                            const double moveAmount = std::min(totalMass, availableSpace);
                            const double dirtAmount = moveAmount * dirtProportion;
                            const double waterAmount = moveAmount * waterProportion;

                            moves.push_back(DirtMove{ .fromX = x,
                                                      .fromY = y,
                                                      .toX = static_cast<uint32_t>(checkX),
                                                      .toY = static_cast<uint32_t>(checkY),
                                                      .dirtAmount = dirtAmount,
                                                      .waterAmount = waterAmount,
                                                      .comOffset = comOffset });
                            LOG_DEBUG(
                                "  Queued diagonal move: from=("
                                << x << "," << y << ") to=(" << checkX << "," << checkY
                                << "), dirt=" << dirtAmount << ", water=" << waterAmount);
                            transferOccurred = true;
                        }
                        else {
                            // Diagonal transfer blocked by full cell, reflect both velocity
                            // components and clamp COM
                            cell.v.x = -cell.v.x * World::ELASTICITY_FACTOR;
                            cell.v.y = -cell.v.y * World::ELASTICITY_FACTOR;
                            // Clamp COM to prevent getting stuck at boundary
                            if (targetX > x) {
                                cell.com.x = COM_DEFLECTION_THRESHOLD; // Hit right obstacle
                            }
                            else {
                                cell.com.x = -COM_DEFLECTION_THRESHOLD; // Hit left obstacle
                            }
                            if (targetY > y) {
                                cell.com.y = COM_DEFLECTION_THRESHOLD; // Hit down obstacle
                            }
                            else {
                                cell.com.y = -COM_DEFLECTION_THRESHOLD; // Hit up obstacle
                            }
                            LOG_DEBUG(
                                "  Diagonal transfer blocked by full cell, reflecting both "
                                "velocities and clamping COM to ("
                                << cell.com.x << ", " << cell.com.y << ")");
                        }
                    }
                    else {
                        // Diagonal transfer blocked by boundary, reflect appropriate components
                        if (!xInBounds) {
                            cell.v.x = -cell.v.x * World::ELASTICITY_FACTOR;
                            // Clamp COM to boundary to prevent accumulation
                            if (targetX < 0) {
                                cell.com.x = -COM_DEFLECTION_THRESHOLD; // Hit left boundary
                            }
                            else if (targetX >= static_cast<int>(width)) {
                                cell.com.x = COM_DEFLECTION_THRESHOLD; // Hit right boundary
                            }
                            LOG_DEBUG(
                                "  Diagonal transfer blocked by X boundary, reflecting v.x and "
                                "clamping COM.x to "
                                << cell.com.x);
                        }
                        if (!yInBounds) {
                            cell.v.y = -cell.v.y * World::ELASTICITY_FACTOR;
                            // Clamp COM to boundary to prevent accumulation
                            if (targetY < 0) {
                                cell.com.y = -COM_DEFLECTION_THRESHOLD; // Hit top boundary
                            }
                            else if (targetY >= static_cast<int>(height)) {
                                cell.com.y = COM_DEFLECTION_THRESHOLD; // Hit bottom boundary
                            }
                            LOG_DEBUG(
                                "  Diagonal transfer blocked by Y boundary, reflecting v.y and "
                                "clamping COM.y to "
                                << cell.com.y);
                        }
                        // Continue to try individual transfers for the valid direction
                        shouldTransferX = shouldTransferX && xInBounds;
                        shouldTransferY = shouldTransferY && yInBounds;
                    }
                }
                // Try X transfer if no diagonal transfer occurred or if diagonal was blocked by
                // boundary
                if (shouldTransferX && !transferOccurred) {
                    int checkX = targetX;
                    int checkY = y;
                    if (checkX >= 0 && checkX < width && checkY >= 0 && checkY < height) {
                        Cell& targetCell = at(checkX, checkY);
                        if (targetCell.percentFull() < 1.0) {
                            // Calculate transfer amounts for X direction
                            const double availableSpace = 1.0 - targetCell.percentFull();
                            const double moveAmount = std::min(totalMass, availableSpace);
                            const double dirtAmount = moveAmount * dirtProportion;
                            const double waterAmount = moveAmount * waterProportion;

                            // Create a copy of comOffset for X transfer
                            Vector2d xComOffset = comOffset;
                            xComOffset.y = cell.com.y; // Keep original Y component

                            moves.push_back(DirtMove{ .fromX = x,
                                                      .fromY = y,
                                                      .toX = static_cast<uint32_t>(checkX),
                                                      .toY = static_cast<uint32_t>(checkY),
                                                      .dirtAmount = dirtAmount,
                                                      .waterAmount = waterAmount,
                                                      .comOffset = xComOffset });
                            LOG_DEBUG(
                                "  Queued X move: from=("
                                << x << "," << y << ") to=(" << checkX << "," << checkY
                                << "), dirt=" << dirtAmount << ", water=" << waterAmount);
                            transferOccurred = true;
                        }
                        else {
                            // X transfer blocked by full cell, reflect velocity and clamp COM
                            cell.v.x = -cell.v.x * World::ELASTICITY_FACTOR;
                            // Clamp COM to prevent getting stuck at boundary
                            if (targetX > x) {
                                cell.com.x = COM_DEFLECTION_THRESHOLD; // Hit right obstacle
                            }
                            else {
                                cell.com.x = -COM_DEFLECTION_THRESHOLD; // Hit left obstacle
                            }
                            LOG_DEBUG(
                                "  X transfer blocked by full cell, reflecting v.x and clamping "
                                "COM.x to "
                                << cell.com.x);
                        }
                    }
                    else {
                        // X transfer blocked by boundary, reflect velocity
                        cell.v.x = -cell.v.x * World::ELASTICITY_FACTOR;
                        // Clamp COM to boundary to prevent accumulation
                        if (targetX < 0) {
                            cell.com.x = -COM_DEFLECTION_THRESHOLD; // Hit left boundary
                        }
                        else if (targetX >= static_cast<int>(width)) {
                            cell.com.x = COM_DEFLECTION_THRESHOLD; // Hit right boundary
                        }
                        LOG_DEBUG(
                            "  X transfer blocked by boundary, reflecting v.x and clamping COM.x "
                            "to "
                            << cell.com.x);
                    }
                }

                // Try Y transfer if needed and no X transfer occurred
                if (shouldTransferY && !transferOccurred) {
                    int checkX = x;
                    int checkY = targetY;
                    if (checkX >= 0 && checkX < width && checkY >= 0 && checkY < height) {
                        Cell& targetCell = at(checkX, checkY);
                        if (targetCell.percentFull() < 1.0) {
                            // Calculate transfer amounts for Y direction
                            const double availableSpace = 1.0 - targetCell.percentFull();
                            const double moveAmount = std::min(totalMass, availableSpace);
                            const double dirtAmount = moveAmount * dirtProportion;
                            const double waterAmount = moveAmount * waterProportion;

                            // Create a copy of comOffset for Y transfer
                            Vector2d yComOffset = comOffset;
                            yComOffset.x = cell.com.x; // Keep original X component

                            moves.push_back({ x,
                                              y,
                                              static_cast<uint32_t>(checkX),
                                              static_cast<uint32_t>(checkY),
                                              dirtAmount,
                                              waterAmount,
                                              yComOffset });
                            LOG_DEBUG(
                                "  Queued Y move: from=("
                                << x << "," << y << ") to=(" << checkX << "," << checkY
                                << "), dirt=" << dirtAmount << ", water=" << waterAmount);
                        }
                        else {
                            // Y transfer blocked by full cell, reflect velocity and clamp COM
                            cell.v.y = -cell.v.y * World::ELASTICITY_FACTOR;
                            // Clamp COM to prevent getting stuck at boundary
                            if (targetY > y) {
                                cell.com.y = COM_DEFLECTION_THRESHOLD; // Hit down obstacle
                            }
                            else {
                                cell.com.y = -COM_DEFLECTION_THRESHOLD; // Hit up obstacle
                            }
                            LOG_DEBUG(
                                "  Y transfer blocked by full cell, reflecting v.y and clamping "
                                "COM.y to "
                                << cell.com.y);
                        }
                    }
                    else {
                        // Y transfer blocked by boundary, reflect velocity
                        cell.v.y = -cell.v.y * World::ELASTICITY_FACTOR;
                        // Clamp COM to boundary to prevent accumulation
                        if (targetY < 0) {
                            cell.com.y = -COM_DEFLECTION_THRESHOLD; // Hit top boundary
                        }
                        else if (targetY >= static_cast<int>(height)) {
                            cell.com.y = COM_DEFLECTION_THRESHOLD; // Hit bottom boundary
                        }
                        LOG_DEBUG(
                            "  Y transfer blocked by boundary, reflecting v.y and clamping COM.y "
                            "to "
                            << cell.com.y);
                    }
                }
            }
            else {
                // No transfer needed, move COM internally
                cell.update(cell.dirt, predictedCom, cell.v);
            }
        }
    }

    applyMoves();

    updateAllPressures(deltaTimeSeconds);

    applyPressure(deltaTimeSeconds);

    applyMoves(); // Apply the pressure moves

    // Update total mass after all moves are complete.
    totalMass = 0.0;
    for (uint32_t y = 0; y < height; y++) {
        for (uint32_t x = 0; x < width; x++) {
            totalMass += at(x, y).percentFull();
        }
    }

    timers.stopTimer("advance_time");
}

void World::applyPressure(const double deltaTimeSeconds)
{
    for (uint32_t y = 0; y < height; y++) {
        for (uint32_t x = 0; x < width; x++) {
            Cell& cell = at(x, y);

            if (cell.pressure.mag() < 0.001) continue;

            double bestScore = -1e9;
            double bestAvailableSpace = 0.0;
            int bestTargetX = x;
            int bestTargetY = y;

            // Check all 8 neighboring cells.
            // Target cell is the one that maximizes a score based on emptiness and "lowness"
            for (int dy = -1; dy <= 1; dy++) {
                for (int dx = -1; dx <= 1; dx++) {
                    if (dx == 0 && dy == 0) continue;

                    int nx = x + dx;
                    int ny = y + dy;
                    if (nx >= 0 && nx < static_cast<int>(width) && ny >= 0
                        && ny < static_cast<int>(height)) {
                        Cell& neighbor = at(nx, ny);
                        double availableSpace = 1.0 - neighbor.percentFull();

                        // Score combines available space and "lowness" (preferring dy=1)
                        // A small weight for dy makes "lowness" a secondary factor.
                        double score = availableSpace + 0.1 * dy;

                        if (score > bestScore) {
                            bestScore = score;
                            bestTargetX = nx;
                            bestTargetY = ny;
                            bestAvailableSpace = availableSpace;
                        }
                    }
                }
            }

            // Calculate movement amount based on cell properties and timestep
            double pressureFactor = cell.pressure.mag();
            double timeFactor = deltaTimeSeconds;
            double dirtAmount = cell.dirt * bestAvailableSpace * pressureFactor * timeFactor;
            double waterAmount = cell.water * bestAvailableSpace * pressureFactor * timeFactor;

            // Queue a partial move using the existing 'moves' member
            // moves.emplace_back(DirtMove{
            //     .fromX = x,
            //     .fromY = y,
            //     .toX = static_cast<uint32_t>(bestTargetX),
            //     .toY = static_cast<uint32_t>(bestTargetY),
            //     .dirtAmount = dirtAmount,
            //     .waterAmount = waterAmount,
            //     .comOffset = Vector2d(0, 0)
            // });

            // Clear pressure for next frame
            cell.pressure = Vector2d(0.0, 0.0);
        }
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

    // Then calculate pressures that cells exert on their neighbor.
    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            Cell& cell = at(x, y);
            if (cell.percentFull() < MIN_DIRT_THRESHOLD) continue;
            double mass = cell.percentFull();
            // Right neighbor
            if (x + 1 < width) {
                if (cell.com.x > 0.0) {
                    Cell& neighbor = at(x + 1, y);
                    neighbor.pressure.x += cell.com.x * mass * deltaTimeSeconds;
                }
            }
            // Left neighbor
            if (x > 0) {
                if (cell.com.x < 0.0) {
                    Cell& neighbor = at(x - 1, y);
                    neighbor.pressure.x += -cell.com.x * mass * deltaTimeSeconds;
                }
            }
            // Down neighbor
            if (y + 1 < height) {
                if (cell.com.y > 0.0) {
                    Cell& neighbor = at(x, y + 1);
                    neighbor.pressure.y += cell.com.y * mass * deltaTimeSeconds;
                }
            }
            // Up neighbor
            if (y > 0) {
                if (cell.com.y < 0.0) {
                    Cell& neighbor = at(x, y - 1);
                    neighbor.pressure.y += -cell.com.y * mass * deltaTimeSeconds;
                }
            }
        }
    }

    // Iterative pressure propagation
    const int numPressureIterations = 8;
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
