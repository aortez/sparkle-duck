#include "WorldSetup.h"
#include "Cell.h"
#include "MaterialType.h"
#include "Vector2d.h"
#include "WorldInterface.h"
#include "spdlog/spdlog.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <random>

void WorldSetup::fillLowerRightQuadrant(WorldInterface& world)
{
    uint32_t startX = world.getWidth() / 2;
    uint32_t startY = world.getHeight() / 2;
    uint32_t sizeX = world.getWidth() - startX;
    uint32_t sizeY = world.getHeight() - startY;

    spdlog::info("Filling lower right quadrant with dirt ({}x{} cells)", sizeX, sizeY);

    for (uint32_t y = startY; y < world.getHeight(); ++y) {
        for (uint32_t x = startX; x < world.getWidth(); ++x) {
            // Use cell coordinates directly.
            world.addMaterialAtCell(x, y, MaterialType::DIRT, 1.0);
        }
    }
}

void WorldSetup::makeWalls(WorldInterface& world)
{
    // Wall creation is now handled by each world implementation internally.
    // World and World have their own wall systems (different material types).
    // This method is kept for interface compatibility but delegates to world implementation.
    spdlog::info(
        "World walls handled by implementation ({}x{} boundary)",
        world.getWidth(),
        world.getHeight());

    // Note: Walls are controlled via setWallsEnabled() and handled in each world's reset/setup.
}

void WorldSetup::makeMiddleMetalWall(WorldInterface& world)
{
    // Add metal wall from top middle to center (World-specific feature).
    uint32_t middle_x = world.getWidth() / 2;
    uint32_t wall_height = world.getHeight() / 2;
    spdlog::info("Adding metal wall at x={} from top to y={}", middle_x, wall_height);

    for (uint32_t y = 0; y < wall_height; y++) {
        // Use cell coordinates directly.
        world.addMaterialAtCell(middle_x, y, MaterialType::METAL, 1.0);
    }
}

void WorldSetup::fillWithDirt(WorldInterface& world)
{
    spdlog::info(
        "Filling entire world with dirt ({}x{} cells)", world.getWidth(), world.getHeight());
    for (uint32_t y = 0; y < world.getHeight(); y++) {
        for (uint32_t x = 0; x < world.getWidth(); x++) {
            // Use cell coordinates directly.
            world.addMaterialAtCell(x, y, MaterialType::DIRT, 1.0);
        }
    }
}

void DefaultWorldSetup::setup(WorldInterface& world)
{
    fillLowerRightQuadrant(world);
    makeWalls(world);
}

void DefaultWorldSetup::addParticles(
    WorldInterface& world, uint32_t timestep, double deltaTimeSeconds)
{
    // Now using CellInterface for direct cell access.
    static double lastSimTime = 0.0;
    static struct EventState {
        double nextTopDrop = 0.33;       // First top drop at 0.33s
        double nextInitialThrow = 0.17;  // First throw at 0.17s
        double nextPeriodicThrow = 0.83; // First periodic throw at 0.83s
        double nextRightThrow = 1.0;     // First right throw at 1.0s
        bool initialThrowDone = false;   // Track if initial throw has happened.
        bool topDropDone = false;        // Track if top drop has happened.
    } eventState;

    const double simTime = lastSimTime + deltaTimeSeconds;

    spdlog::debug(
        "DefaultWorldSetup timestep {}: simTime={:.3f}, lastSimTime={:.3f}, deltaTime={:.3f}",
        timestep,
        simTime,
        lastSimTime,
        deltaTimeSeconds);

    // Drop a dirt from the top.
    if (!eventState.topDropDone && simTime >= eventState.nextTopDrop) {
        spdlog::info("Adding top drop at time {:.3f}s", simTime);
        uint32_t centerX = world.getWidth() / 2;
        CellInterface& cell =
            world.getCellInterface(centerX, 1); // 1 to be just below the top wall.
        cell.addDirt(1.0);
        eventState.topDropDone = true;
    }

    // Initial throw from left center.
    if (!eventState.initialThrowDone && simTime >= eventState.nextInitialThrow) {
        spdlog::info("Adding initial throw at time {:.3f}s", simTime);
        uint32_t centerY = world.getHeight() / 2;
        CellInterface& cell = world.getCellInterface(2, centerY); // Against the left wall.
        cell.addDirtWithVelocity(1.0, Vector2d(5, -5));
        eventState.initialThrowDone = true;
    }

    // Recurring throws every ~0.83 seconds
    const double period = 0.83;
    if (simTime >= eventState.nextPeriodicThrow) {
        spdlog::debug("Adding periodic throw at time {:.3f}s", simTime);
        uint32_t centerY = world.getHeight() / 2;
        CellInterface& cell = world.getCellInterface(2, centerY); // Against the left wall.
        cell.addDirtWithVelocity(1.0, Vector2d(10, -10));
        // Schedule next throw.
        eventState.nextPeriodicThrow += period;
    }

    // Recurring throws from right side every ~0.83 seconds
    if (simTime >= eventState.nextRightThrow) {
        spdlog::debug("Adding right periodic throw at time {:.3f}s", simTime);
        uint32_t centerY = world.getHeight() / 2 - 2;
        CellInterface& cell =
            world.getCellInterface(world.getWidth() - 3, centerY); // Against the right wall.
        cell.addDirtWithVelocity(1.0, Vector2d(-10, -10));
        // Schedule next throw.
        eventState.nextRightThrow += period;
    }

    lastSimTime = simTime;

    /* TEMPORARILY DISABLED - Complex particle logic with cell access
    static double lastSimTime = 0.0;
    static struct EventState {
        double nextTopDrop = 0.33;       // First top drop at 0.33s
        double nextInitialThrow = 0.17;  // First throw at 0.17s
        double nextPeriodicThrow = 0.83; // First periodic throw at 0.83s
        double nextRightThrow = 1.0;     // First right throw at 1.0s
        double sweepTime = 0.0;          // Time tracking for sweep.
        double beatTime = 0.0;           // Time tracking for beats.
        bool initialThrowDone = false;   // Track if initial throw has happened.
        bool topDropDone = false;        // Track if top drop has happened.
    } eventState;

    const double simTime = lastSimTime + deltaTimeSeconds;

    spdlog::debug(
        "DefaultWorldSetup timestep {}: simTime={:.3f}, lastSimTime={:.3f}, deltaTime={:.3f}",
        timestep,
        simTime,
        lastSimTime,
        deltaTimeSeconds);

    // Constants for sweep behavior.
    const double SWEEP_PERIOD = 2.0; // Time for one complete sweep (left to right and back).
    const double DIRT_PERIOD = 0.5;  // Period for dirt amount oscillation.
    const double SWEEP_SPEED = 2.0;  // Speed of the sweep.
    const double DIRT_AMPLITUDE =
        0.5; // Amplitude of dirt oscillation (0.5 means dirt varies from 0.5 to 1.0)
    const double BEAT_PERIOD = 0.5;  // Length of one beat in seconds.
    const int BEATS_PER_PATTERN = 8; // Total beats in the pattern.
    const int BEATS_ON = 2;          // Number of beats the emitter is on.

    // Update beat time.
    eventState.beatTime += deltaTimeSeconds;
    if (eventState.beatTime >= BEAT_PERIOD * BEATS_PER_PATTERN) {
        eventState.beatTime -= BEAT_PERIOD * BEATS_PER_PATTERN;
    }

    // Calculate current beat in the pattern (0 to BEATS_PER_PATTERN-1).
    int currentBeat = static_cast<int>(eventState.beatTime / BEAT_PERIOD);
    // bool isEmitterOn = currentBeat < BEATS_ON;
    bool isEmitterOn = false;

    // Only update sweep and emit particles if the emitter is on.
    if (isEmitterOn) {
        // Update sweep time.
        eventState.sweepTime += deltaTimeSeconds;

        // Calculate sweep position (x coordinate).
        double sweepPhase = (eventState.sweepTime / SWEEP_PERIOD) * 2.0 * M_PI;
        double sweepX = (std::sin(sweepPhase) + 1.0) * 0.5; // Maps to [0,1]
        uint32_t xPos =
            static_cast<uint32_t>(sweepX * (world.getWidth() - 2)) + 1; // Maps to [1,width-2]

        // Calculate dirt amount oscillation.
        double dirtPhase = (eventState.sweepTime / DIRT_PERIOD) * 2.0 * M_PI;
        double dirtAmount =
            0.5 + DIRT_AMPLITUDE * std::sin(dirtPhase); // Oscillates between 0.5 and 1.0

        // Emit particle at current sweep position.
        Cell& cell = world.at(xPos, 1); // 1 to be just below the top wall.
        cell.update(dirtAmount, Vector2d(0.0, 0.0), Vector2d(0.0, 0.0));
        spdlog::debug(
            "Sweep emitter at x={} with dirt={:.2f} (beat {})", xPos, dirtAmount, currentBeat);
    }

    // Drop a dirt from the top.
    if (!eventState.topDropDone && simTime >= eventState.nextTopDrop) {
        spdlog::info("Adding top drop at time {:.3f}s", simTime);
        uint32_t centerX = world.getWidth() / 2;
        Cell& cell = world.at(centerX, 1); // 1 to be just below the top wall.
        cell.update(1.0, Vector2d(0.0, 0.0), Vector2d(0.0, 0.0));
        eventState.topDropDone = true;
    }

    // Initial throw from left center.
    if (!eventState.initialThrowDone && simTime >= eventState.nextInitialThrow) {
        spdlog::info("Adding initial throw at time {:.3f}s", simTime);
        uint32_t centerY = world.getHeight() / 2;
        Cell& cell = world.at(2, centerY); // Against the left wall.
        cell.update(1.0, Vector2d(0.0, 0.0), Vector2d(5, -5));
        eventState.initialThrowDone = true;
    }

    // Recurring throws every ~0.83 seconds
    const double period = 0.83;
    if (simTime >= eventState.nextPeriodicThrow) {
        spdlog::debug("Adding periodic throw at time {:.3f}s", simTime);
        uint32_t centerY = world.getHeight() / 2;
        Cell& cell = world.at(2, centerY); // Against the left wall.
        cell.update(1.0, Vector2d(0.0, 0.0), Vector2d(10, -10));
        // Schedule next throw.
        eventState.nextPeriodicThrow += period;
    }

    // Recurring throws from right side every ~0.83 seconds
    if (simTime >= eventState.nextRightThrow) {
        spdlog::debug("Adding right periodic throw at time {:.3f}s", simTime);
        uint32_t centerY = world.getHeight() / 2 - 2;
        Cell& cell = world.at(world.getWidth() - 3, centerY); // Against the right wall.
        cell.update(1.0, Vector2d(0.0, 0.0), Vector2d(-10, -10));
        // Schedule next throw.
        eventState.nextRightThrow += period;
    }

    lastSimTime = simTime;
    */ // END DISABLED COMPLEX PARTICLE LOGIC.
}

DefaultWorldSetup::~DefaultWorldSetup()
{}

// ConfigurableWorldSetup implementation.
void ConfigurableWorldSetup::setup(WorldInterface& world)
{
    spdlog::info("ConfigurableWorldSetup::setup called - waterColumnEnabled={}", waterColumnEnabled);

    if (lowerRightQuadrantEnabled) {
        fillLowerRightQuadrant(world);
    }
    if (wallsEnabled) {
        makeWalls(world);
    }
    if (middleMetalWallEnabled) {
        makeMiddleMetalWall(world);
    }
    if (waterColumnEnabled) {
        // Add 5-wide × 20-tall water column on left side (top 20 cells).
        spdlog::info("Adding water column (5 wide × 20 tall) on left side");
        for (uint32_t y = 0; y < 20 && y < world.getHeight(); ++y) {
            for (uint32_t x = 1; x <= 5 && x < world.getWidth(); ++x) {
                CellInterface& cell = world.getCellInterface(x, y);
                cell.addWater(1.0);  // Add full cell of water.
            }
        }
    } else {
        spdlog::info("Water column NOT enabled - skipping");
    }
}

void ConfigurableWorldSetup::addParticles(
    WorldInterface& world, uint32_t timestep, double deltaTimeSeconds)
{
    // Now using CellInterface for direct cell access.
    static double lastSimTime = 0.0;
    static struct EventState {
        double nextTopDrop = 0.33;       // First top drop at 0.33s
        double nextInitialThrow = 0.17;  // First throw at 0.17s
        double nextPeriodicThrow = 0.83; // First periodic throw at 0.83s
        double nextRightThrow = 1.0;     // First right throw at 1.0s
        bool initialThrowDone = false;   // Track if initial throw has happened.
        bool topDropDone = false;        // Track if top drop has happened.
        double nextRainDrop = 0.0;       // Time for next rain drop.
    } eventState;

    const double simTime = lastSimTime + deltaTimeSeconds;

    spdlog::debug(
        "ConfigurableWorldSetup timestep {}: simTime={:.3f}, lastSimTime={:.3f}, deltaTime={:.3f}",
        timestep,
        simTime,
        lastSimTime,
        deltaTimeSeconds);

    // Drop a dirt from the top (if enabled).
    if (topDropEnabled && !eventState.topDropDone && simTime >= eventState.nextTopDrop) {
        spdlog::info("Adding top drop at time {:.3f}s", simTime);
        uint32_t centerX = world.getWidth() / 2;
        CellInterface& cell =
            world.getCellInterface(centerX, 1); // 1 to be just below the top wall.
        cell.addDirt(1.0);
        eventState.topDropDone = true;
    }

    // Initial throw from left center (if enabled).
    if (leftThrowEnabled && !eventState.initialThrowDone
        && simTime >= eventState.nextInitialThrow) {
        spdlog::info("Adding initial throw at time {:.3f}s", simTime);
        uint32_t centerY = world.getHeight() / 2;
        if (2 < world.getWidth() && centerY < world.getHeight()) {
            CellInterface& cell = world.getCellInterface(2, centerY); // Against the left wall.
            cell.addDirtWithVelocity(1.0, Vector2d(5, -5));
        }
        eventState.initialThrowDone = true;
    }

    // Recurring throws every ~0.83 seconds (if left throw enabled)
    const double period = 0.83;
    if (leftThrowEnabled && simTime >= eventState.nextPeriodicThrow) {
        spdlog::debug("Adding periodic throw at time {:.3f}s", simTime);
        uint32_t centerY = world.getHeight() / 2;
        if (2 < world.getWidth() && centerY < world.getHeight()) {
            CellInterface& cell = world.getCellInterface(2, centerY); // Against the left wall.
            cell.addDirtWithVelocity(1.0, Vector2d(10, -10));
        }
        // Schedule next throw.
        eventState.nextPeriodicThrow += period;
    }

    // Recurring throws from right side every ~0.83 seconds (if right throw enabled)
    if (rightThrowEnabled && simTime >= eventState.nextRightThrow) {
        spdlog::debug("Adding right periodic throw at time {:.3f}s", simTime);
        uint32_t rightX = world.getWidth() - 3;
        int32_t centerYSigned = static_cast<int32_t>(world.getHeight()) / 2 - 2;
        if (rightX < world.getWidth() && centerYSigned >= 0) {
            uint32_t centerY = static_cast<uint32_t>(centerYSigned);
            CellInterface& cell =
                world.getCellInterface(rightX, centerY); // Against the right wall.
            cell.addDirtWithVelocity(1.0, Vector2d(-10, -10));
        }
        // Schedule next throw.
        eventState.nextRightThrow += period;
    }

    // Rain drops at variable rate (if rain rate > 0).
    if (rainRate > 0.0 && simTime >= eventState.nextRainDrop) {
        spdlog::debug("Adding rain drop at time {:.3f}s (rate: {:.1f}/s)", simTime, rainRate);

        // Use normal distribution for horizontal position.
        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::normal_distribution<double> normalDist(0.5, 0.15); // mean=0.5, stddev=0.15

        // Generate random position across the top, clamped to world bounds.
        double randomPos = std::max(0.0, std::min(1.0, normalDist(gen)));
        uint32_t xPos =
            static_cast<uint32_t>(randomPos * (world.getWidth() - 2)) + 1; // [1, width-2]

        if (xPos < world.getWidth() && 1 < world.getHeight()) {
            CellInterface& cell = world.getCellInterface(xPos, 1); // Just below the top wall.
            // Add water instead of dirt for rain.
            cell.addWater(0.8);
        }

        // Schedule next rain drop based on current rate.
        double intervalSeconds = 1.0 / rainRate;
        eventState.nextRainDrop = simTime + intervalSeconds;
    }

    lastSimTime = simTime;
}

// Feature-preserving resize implementation.
std::vector<WorldSetup::ResizeData> WorldSetup::captureWorldState(
    const WorldInterface& /* world. */) const
{
    // Resize functionality not available for WorldInterface - requires direct cell access.
    // Return empty state for now - TODO: implement resize support if needed.
    spdlog::warn("captureWorldState not implemented for WorldInterface - resize not supported");
    return {};
}

void WorldSetup::applyWorldState(
    WorldInterface& world,
    const std::vector<ResizeData>& oldState,
    uint32_t oldWidth,
    uint32_t oldHeight) const
{
    uint32_t newWidth = world.getWidth();
    uint32_t newHeight = world.getHeight();

    // Calculate scaling factors.
    double scaleX = static_cast<double>(oldWidth) / newWidth;
    double scaleY = static_cast<double>(oldHeight) / newHeight;

    for (uint32_t y = 0; y < newHeight; y++) {
        for (uint32_t x = 0; x < newWidth; x++) {
            // Map new cell coordinates back to old coordinate space.
            double oldX = (x + 0.5) * scaleX - 0.5;
            double oldY = (y + 0.5) * scaleY - 0.5;

            // Calculate edge strength at the old position for adaptive interpolation.
            double edgeStrength = calculateEdgeStrength(
                oldState,
                oldWidth,
                oldHeight,
                static_cast<uint32_t>(std::round(oldX)),
                static_cast<uint32_t>(std::round(oldY)));

            // Interpolate the cell data.
            ResizeData newData __attribute__((unused)) =
                interpolateCell(oldState, oldWidth, oldHeight, oldX, oldY, edgeStrength);

            // Apply the interpolated data to the new cell.
            // TODO: WorldInterface doesn't support direct cell access - resize not supported.
            spdlog::warn("Resize not supported for WorldInterface");
        }
    }
}

double WorldSetup::calculateEdgeStrength(
    const std::vector<ResizeData>& state,
    uint32_t width,
    uint32_t height,
    uint32_t x,
    uint32_t y) const
{
    // Clamp coordinates to valid range.
    x = std::min(x, width - 1);
    y = std::min(y, height - 1);

    // Use Sobel operator to detect edges based on mass density.
    double sobelX = 0.0, sobelY = 0.0;

    // Sobel X kernel: [-1 0 1; -2 0 2; -1 0 1]
    // Sobel Y kernel: [-1 -2 -1; 0 0 0; 1 2 1]
    for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            int nx = static_cast<int>(x) + dx;
            int ny = static_cast<int>(y) + dy;

            // Clamp to bounds.
            nx = std::max(0, std::min(nx, static_cast<int>(width) - 1));
            ny = std::max(0, std::min(ny, static_cast<int>(height) - 1));

            double mass = state[ny * width + nx].dirt + state[ny * width + nx].water;

            // Sobel X weights.
            int sobelXWeight = 0;
            if (dx == -1)
                sobelXWeight = (dy == 0) ? -2 : -1;
            else if (dx == 1)
                sobelXWeight = (dy == 0) ? 2 : 1;

            // Sobel Y weights.
            int sobelYWeight = 0;
            if (dy == -1)
                sobelYWeight = (dx == 0) ? -2 : -1;
            else if (dy == 1)
                sobelYWeight = (dx == 0) ? 2 : 1;

            sobelX += mass * sobelXWeight;
            sobelY += mass * sobelYWeight;
        }
    }

    // Calculate edge magnitude and normalize.
    double edgeMagnitude = std::sqrt(sobelX * sobelX + sobelY * sobelY);
    return std::min(1.0, edgeMagnitude * 2.0); // Scale and clamp to [0,1]
}

WorldSetup::ResizeData WorldSetup::interpolateCell(
    const std::vector<ResizeData>& oldState,
    uint32_t oldWidth,
    uint32_t oldHeight,
    double newX,
    double newY,
    double edgeStrength) const
{
    // Adaptive interpolation: use nearest neighbor for strong edges, bilinear for smooth areas.
    const double EDGE_THRESHOLD = 0.3;

    if (edgeStrength > EDGE_THRESHOLD) {
        // Strong edge: use nearest neighbor to preserve sharp features.
        double blendFactor = (edgeStrength - EDGE_THRESHOLD) / (1.0 - EDGE_THRESHOLD);
        ResizeData nearest = nearestNeighborSample(oldState, oldWidth, oldHeight, newX, newY);
        ResizeData bilinear = bilinearInterpolate(oldState, oldWidth, oldHeight, newX, newY);

        // Blend between bilinear and nearest neighbor based on edge strength.
        ResizeData result;
        result.dirt = bilinear.dirt * (1.0 - blendFactor) + nearest.dirt * blendFactor;
        result.water = bilinear.water * (1.0 - blendFactor) + nearest.water * blendFactor;
        result.com = bilinear.com * (1.0 - blendFactor) + nearest.com * blendFactor;
        result.velocity = bilinear.velocity * (1.0 - blendFactor) + nearest.velocity * blendFactor;
        return result;
    }
    else {
        // Smooth area: use bilinear interpolation.
        return bilinearInterpolate(oldState, oldWidth, oldHeight, newX, newY);
    }
}

WorldSetup::ResizeData WorldSetup::bilinearInterpolate(
    const std::vector<ResizeData>& oldState,
    uint32_t oldWidth,
    uint32_t oldHeight,
    double x,
    double y) const
{
    // Clamp to valid range.
    x = std::max(0.0, std::min(x, static_cast<double>(oldWidth) - 1.0));
    y = std::max(0.0, std::min(y, static_cast<double>(oldHeight) - 1.0));

    // Get integer coordinates and fractions.
    int x0 = static_cast<int>(std::floor(x));
    int y0 = static_cast<int>(std::floor(y));
    int x1 = std::min(x0 + 1, static_cast<int>(oldWidth) - 1);
    int y1 = std::min(y0 + 1, static_cast<int>(oldHeight) - 1);

    double fx = x - x0;
    double fy = y - y0;

    // Get the four surrounding samples.
    const ResizeData& s00 = oldState[y0 * oldWidth + x0];
    const ResizeData& s10 = oldState[y0 * oldWidth + x1];
    const ResizeData& s01 = oldState[y1 * oldWidth + x0];
    const ResizeData& s11 = oldState[y1 * oldWidth + x1];

    // Bilinear interpolation.
    ResizeData result;
    result.dirt = s00.dirt * (1 - fx) * (1 - fy) + s10.dirt * fx * (1 - fy)
        + s01.dirt * (1 - fx) * fy + s11.dirt * fx * fy;
    result.water = s00.water * (1 - fx) * (1 - fy) + s10.water * fx * (1 - fy)
        + s01.water * (1 - fx) * fy + s11.water * fx * fy;
    result.com = s00.com * (1 - fx) * (1 - fy) + s10.com * fx * (1 - fy) + s01.com * (1 - fx) * fy
        + s11.com * fx * fy;
    result.velocity = s00.velocity * (1 - fx) * (1 - fy) + s10.velocity * fx * (1 - fy)
        + s01.velocity * (1 - fx) * fy + s11.velocity * fx * fy;

    return result;
}

WorldSetup::ResizeData WorldSetup::nearestNeighborSample(
    const std::vector<ResizeData>& oldState,
    uint32_t oldWidth,
    uint32_t oldHeight,
    double x,
    double y) const
{
    // Clamp and round to nearest integer coordinates.
    int nx = std::max(0, std::min(static_cast<int>(std::round(x)), static_cast<int>(oldWidth) - 1));
    int ny =
        std::max(0, std::min(static_cast<int>(std::round(y)), static_cast<int>(oldHeight) - 1));

    return oldState[ny * oldWidth + nx];
}
