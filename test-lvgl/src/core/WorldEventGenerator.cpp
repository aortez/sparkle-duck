#include "WorldEventGenerator.h"
#include "Cell.h"
#include "MaterialType.h"
#include "PhysicsSettings.h"
#include "Vector2d.h"
#include "World.h"
#include "WorldData.h"
#include "spdlog/spdlog.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <random>

using namespace DirtSim;

void WorldEventGenerator::fillLowerRightQuadrant(World& world)
{
    uint32_t startX = world.getData().width / 2;
    uint32_t startY = world.getData().height / 2;
    uint32_t sizeX = world.getData().width - startX;
    uint32_t sizeY = world.getData().height - startY;

    spdlog::info("Filling lower right quadrant with dirt ({}x{} cells)", sizeX, sizeY);

    for (uint32_t y = startY; y < world.getData().height; ++y) {
        for (uint32_t x = startX; x < world.getData().width; ++x) {
            // Use cell coordinates directly.
            world.addMaterialAtCell(x, y, MaterialType::DIRT, 1.0);
        }
    }
}

void WorldEventGenerator::makeWalls(World& world)
{
    // Wall creation is now handled by each world implementation internally.
    // World and World have their own wall systems (different material types).
    // This method is kept for interface compatibility but delegates to world implementation.
    spdlog::info(
        "World walls handled by implementation ({}x{} boundary)",
        world.getData().width,
        world.getData().height);

    // Note: Walls are controlled via setWallsEnabled() and handled in each world's reset/setup.
}

void WorldEventGenerator::makeMiddleMetalWall(World& world)
{
    // Add metal wall from top middle to center (World-specific feature).
    uint32_t middle_x = world.getData().width / 2;
    uint32_t wall_height = world.getData().height / 2;
    spdlog::info("Adding metal wall at x={} from top to y={}", middle_x, wall_height);

    for (uint32_t y = 0; y < wall_height; y++) {
        // Use cell coordinates directly.
        world.addMaterialAtCell(middle_x, y, MaterialType::METAL, 1.0);
    }
}

void WorldEventGenerator::fillWithDirt(World& world)
{
    spdlog::info(
        "Filling entire world with dirt ({}x{} cells)",
        world.getData().width,
        world.getData().height);
    for (uint32_t y = 0; y < world.getData().height; y++) {
        for (uint32_t x = 0; x < world.getData().width; x++) {
            // Use cell coordinates directly.
            world.addMaterialAtCell(x, y, MaterialType::DIRT, 1.0);
        }
    }
}

void WorldEventGenerator::dirtQuadrantToggle(World& world, bool enabled)
{
    uint32_t startX = world.getData().width / 2;
    uint32_t startY = world.getData().height / 2;
    uint32_t sizeX = world.getData().width - startX;
    uint32_t sizeY = world.getData().height - startY;

    if (enabled) {
        spdlog::info("Adding dirt quadrant ({}x{} cells)", sizeX, sizeY);
        for (uint32_t y = startY; y < world.getData().height; ++y) {
            for (uint32_t x = startX; x < world.getData().width; ++x) {
                world.addMaterialAtCell(x, y, MaterialType::DIRT, 1.0);
            }
        }
    }
    else {
        spdlog::info("Removing dirt quadrant ({}x{} cells)", sizeX, sizeY);
        for (uint32_t y = startY; y < world.getData().height; ++y) {
            for (uint32_t x = startX; x < world.getData().width; ++x) {
                Cell& cell = world.getData().at(x, y);
                cell.clear();
            }
        }
    }
}

void WorldEventGenerator::waterColumnToggle(World& world, bool enabled)
{
    // Scale water column dimensions based on world size.
    // Width: 5% of world width, minimum 3, maximum 8.
    uint32_t columnWidth = std::max(3u, std::min(8u, world.getData().width / 20));
    // Height: Top 1/3 of world height.
    uint32_t columnHeight = world.getData().height / 3;

    if (enabled) {
        spdlog::info(
            "Adding water column ({} wide × {} tall) on left side", columnWidth, columnHeight);
        for (uint32_t y = 0; y < columnHeight && y < world.getData().height; ++y) {
            for (uint32_t x = 1; x <= columnWidth && x < world.getData().width; ++x) {
                Cell& cell = world.getData().at(x, y);
                cell.addWater(1.0);
            }
        }
    }
    else {
        spdlog::info(
            "Removing water column ({} wide × {} tall) on left side", columnWidth, columnHeight);
        for (uint32_t y = 0; y < columnHeight && y < world.getData().height; ++y) {
            for (uint32_t x = 1; x <= columnWidth && x < world.getData().width; ++x) {
                Cell& cell = world.getData().at(x, y);
                cell.clear();
            }
        }
    }
}

void DefaultWorldEventGenerator::clear(World& world)
{
    // Reset all cells to empty state.
    for (uint32_t y = 0; y < world.getData().height; ++y) {
        for (uint32_t x = 0; x < world.getData().width; ++x) {
            world.getData().at(x, y) = Cell(); // Default empty cell.
        }
    }
    spdlog::info("World cleared to empty state");
}

void DefaultWorldEventGenerator::setup(World& world)
{
    fillLowerRightQuadrant(world);
    makeWalls(world);
}

std::unique_ptr<WorldEventGenerator> DefaultWorldEventGenerator::clone() const
{
    auto cloned = std::make_unique<DefaultWorldEventGenerator>();
    cloned->lastSimTime = lastSimTime;
    cloned->nextInitialThrow = nextInitialThrow;
    cloned->nextPeriodicThrow = nextPeriodicThrow;
    cloned->nextRightThrow = nextRightThrow;
    cloned->initialThrowDone = initialThrowDone;
    return cloned;
}

void DefaultWorldEventGenerator::addParticles(
    World& world, uint32_t timestep, double deltaTimeSeconds)
{
    // Now using Cell for direct cell access.
    const double simTime = lastSimTime + deltaTimeSeconds;

    spdlog::debug(
        "DefaultWorldEventGenerator timestep {}: simTime={:.3f}, lastSimTime={:.3f}, "
        "deltaTime={:.3f}",
        timestep,
        simTime,
        lastSimTime,
        deltaTimeSeconds);

    // Initial throw from left center.
    if (!initialThrowDone && simTime >= nextInitialThrow) {
        spdlog::info("Adding initial throw at time {:.3f}s", simTime);
        uint32_t centerY = world.getData().height / 2;
        Cell& cell = world.getData().at(2, centerY); // Against the left wall.
        cell.addDirtWithVelocity(1.0, Vector2d{ 5, -5 });
        initialThrowDone = true;
    }

    // Recurring throws every ~0.83 seconds
    const double period = 0.83;
    if (simTime >= nextPeriodicThrow) {
        spdlog::debug("Adding periodic throw at time {:.3f}s", simTime);
        uint32_t centerY = world.getData().height / 2;
        Cell& cell = world.getData().at(2, centerY); // Against the left wall.
        cell.addDirtWithVelocity(1.0, Vector2d{ 10, -10 });
        // Schedule next throw.
        nextPeriodicThrow += period;
    }

    // Recurring throws from right side every ~0.83 seconds
    if (simTime >= nextRightThrow) {
        spdlog::debug("Adding right periodic throw at time {:.3f}s", simTime);
        uint32_t centerY = world.getData().height / 2 - 2;
        Cell& cell =
            world.getData().at(world.getData().width - 3, centerY); // Against the right wall.
        cell.addDirtWithVelocity(1.0, Vector2d{ -50, -10 });
        // Schedule next throw.
        nextRightThrow += period;
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
            static_cast<uint32_t>(sweepX * (world.getData().width - 2)) + 1; // Maps to [1,width-2]

        // Calculate dirt amount oscillation.
        double dirtPhase = (eventState.sweepTime / DIRT_PERIOD) * 2.0 * M_PI;
        double dirtAmount =
            0.5 + DIRT_AMPLITUDE * std::sin(dirtPhase); // Oscillates between 0.5 and 1.0

        // Emit particle at current sweep position.
        Cell& cell = world.getData().at(xPos, 1); // 1 to be just below the top wall.
        cell.update(dirtAmount, Vector2d{0.0, 0.0}, Vector2d{0.0, 0.0});
        spdlog::debug(
            "Sweep emitter at x={} with dirt={:.2f} (beat {})", xPos, dirtAmount, currentBeat);
    }

    // Drop a dirt from the top.
    if (!eventState.topDropDone && simTime >= eventState.nextTopDrop) {
        spdlog::info("Adding top drop at time {:.3f}s", simTime);
        uint32_t centerX = world.getData().width / 2;
        Cell& cell = world.getData().at(centerX, 1); // 1 to be just below the top wall.
        cell.update(1.0, Vector2d{0.0, 0.0}, Vector2d{0.0, 0.0});
        eventState.topDropDone = true;
    }

    // Initial throw from left center.
    if (!eventState.initialThrowDone && simTime >= eventState.nextInitialThrow) {
        spdlog::info("Adding initial throw at time {:.3f}s", simTime);
        uint32_t centerY = world.getData().height / 2;
        Cell& cell = world.getData().at(2, centerY); // Against the left wall.
        cell.update(1.0, Vector2d{0.0, 0.0}, Vector2d{5, -5});
        eventState.initialThrowDone = true;
    }

    // Recurring throws every ~0.83 seconds
    const double period = 0.83;
    if (simTime >= eventState.nextPeriodicThrow) {
        spdlog::debug("Adding periodic throw at time {:.3f}s", simTime);
        uint32_t centerY = world.getData().height / 2;
        Cell& cell = world.getData().at(2, centerY); // Against the left wall.
        cell.update(1.0, Vector2d{0.0, 0.0}, Vector2d{10, -10});
        // Schedule next throw.
        eventState.nextPeriodicThrow += period;
    }

    // Recurring throws from right side every ~0.83 seconds
    if (simTime >= eventState.nextRightThrow) {
        spdlog::debug("Adding right periodic throw at time {:.3f}s", simTime);
        uint32_t centerY = world.getData().height / 2 - 2;
        Cell& cell = world.getData().at(world.getData().width - 3, centerY); // Against the right
    wall. cell.update(1.0, Vector2d{0.0, 0.0}, Vector2d{-10, -10});
        // Schedule next throw.
        eventState.nextRightThrow += period;
    }

    lastSimTime = simTime;
    */ // END DISABLED COMPLEX PARTICLE LOGIC.
}

DefaultWorldEventGenerator::~DefaultWorldEventGenerator()
{}

// ConfigurableWorldEventGenerator implementation.
void ConfigurableWorldEventGenerator::clear(World& world)
{
    // Reset all cells to empty state.
    for (uint32_t y = 0; y < world.getData().height; ++y) {
        for (uint32_t x = 0; x < world.getData().width; ++x) {
            world.getData().at(x, y) = Cell(); // Default empty cell.
        }
    }
    spdlog::info("World cleared to empty state");
}

void ConfigurableWorldEventGenerator::setup(World& world)
{
    spdlog::info(
        "ConfigurableWorldEventGenerator::setup called - waterColumnEnabled={}",
        config_.water_column_enabled);

    // Use toggle methods for initial setup (read from config_).
    dirtQuadrantToggle(world, config_.quadrant_enabled);

    if (wallsEnabled) {
        makeWalls(world);
    }
    if (middleMetalWallEnabled) {
        makeMiddleMetalWall(world);
    }

    waterColumnToggle(world, config_.water_column_enabled);
}

// Constructor: Generator owns the config.
ConfigurableWorldEventGenerator::ConfigurableWorldEventGenerator(const SandboxConfig& config)
    : config_(config)
{
    // If water column is enabled in initial config, set start time to 0.
    // (It will be updated to actual simTime on first addParticles call).
    if (config_.water_column_enabled) {
        waterColumnStartTime = 0.0;
        spdlog::info(
            "Water column enabled in initial config (will auto-disable after {:.1f}s)",
            WATER_COLUMN_DURATION);
    }
}

void ConfigurableWorldEventGenerator::updateConfig(const SandboxConfig& newConfig)
{
    // Check if water column state changed.
    bool wasEnabled = config_.water_column_enabled;
    bool nowEnabled = newConfig.water_column_enabled;

    // Update config.
    config_ = newConfig;

    // If water column was just enabled, record start time.
    if (!wasEnabled && nowEnabled) {
        waterColumnStartTime = lastSimTime;
        spdlog::info(
            "Water column enabled at time {:.3f}s (will auto-disable after {:.1f}s)",
            waterColumnStartTime,
            WATER_COLUMN_DURATION);
    }
    else if (wasEnabled && !nowEnabled) {
        waterColumnStartTime = -1.0;
    }
}

std::unique_ptr<WorldEventGenerator> ConfigurableWorldEventGenerator::clone() const
{
    auto cloned = std::make_unique<ConfigurableWorldEventGenerator>(config_);
    // Copy non-config flags.
    cloned->wallsEnabled = wallsEnabled;
    cloned->middleMetalWallEnabled = middleMetalWallEnabled;
    cloned->leftThrowEnabled = leftThrowEnabled;
    cloned->sweepEnabled = sweepEnabled;
    // Copy event generation state.
    cloned->lastSimTime = lastSimTime;
    cloned->nextInitialThrow = nextInitialThrow;
    cloned->nextPeriodicThrow = nextPeriodicThrow;
    cloned->nextRightThrow = nextRightThrow;
    cloned->nextRainDrop = nextRainDrop;
    cloned->initialThrowDone = initialThrowDone;
    return cloned;
}

void ConfigurableWorldEventGenerator::addParticles(
    World& world, uint32_t timestep, double deltaTimeSeconds)
{
    const double simTime = lastSimTime + deltaTimeSeconds;

    spdlog::debug(
        "ConfigurableWorldEventGenerator timestep {}: simTime={:.3f}, lastSimTime={:.3f}, "
        "deltaTime={:.3f}",
        timestep,
        simTime,
        lastSimTime,
        deltaTimeSeconds);

    // Initial throw from left center (if enabled).
    if (leftThrowEnabled && !initialThrowDone && simTime >= nextInitialThrow) {
        spdlog::info("Adding initial throw at time {:.3f}s", simTime);
        uint32_t centerY = world.getData().height / 2;
        if (2 < world.getData().width && centerY < world.getData().height) {
            Cell& cell = world.getData().at(2, centerY); // Against the left wall.
            cell.addDirtWithVelocity(1.0, Vector2d{ 5, -5 });
        }
        initialThrowDone = true;
    }

    // Recurring throws every ~0.83 seconds (if left throw enabled)
    const double period = 0.83;
    if (leftThrowEnabled && simTime >= nextPeriodicThrow) {
        spdlog::debug("Adding periodic throw at time {:.3f}s", simTime);
        uint32_t centerY = world.getData().height / 2;
        if (2 < world.getData().width && centerY < world.getData().height) {
            Cell& cell = world.getData().at(2, centerY); // Against the left wall.
            cell.addDirtWithVelocity(1.0, Vector2d{ 10, -10 });
        }
        // Schedule next throw.
        nextPeriodicThrow += period;
    }

    // Recurring throws from right side every ~0.83 seconds (if right throw enabled).
    if (config_.right_throw_enabled && simTime >= nextRightThrow) {
        spdlog::debug("Adding right periodic throw at time {:.3f}s", simTime);
        uint32_t rightX = world.getData().width - 3;
        int32_t centerYSigned = static_cast<int32_t>(world.getData().height) / 2 - 2;
        if (rightX < world.getData().width && centerYSigned >= 0) {
            uint32_t centerY = static_cast<uint32_t>(centerYSigned);
            Cell& cell = world.getData().at(rightX, centerY); // Against the right wall.
            cell.addDirtWithVelocity(1.0, Vector2d{ -10, -10 });
        }
        // Schedule next throw.
        nextRightThrow += period;
    }

    // Rain drops at variable rate (if rain rate > 0).
    if (config_.rain_rate > 0.0 && simTime >= nextRainDrop) {
        spdlog::debug(
            "Adding rain drop at time {:.3f}s (rate: {:.1f}/s)", simTime, config_.rain_rate);

        // Use normal distribution for horizontal position.
        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::normal_distribution<double> normalDist(0.5, 0.15); // mean=0.5, stddev=0.15

        // Generate random position across the top, clamped to world bounds.
        double randomPos = std::max(0.0, std::min(1.0, normalDist(gen)));
        uint32_t xPos =
            static_cast<uint32_t>(randomPos * (world.getData().width - 2)) + 1; // [1, width-2]

        if (xPos < world.getData().width && 1 < world.getData().height) {
            Cell& cell = world.getData().at(xPos, 1); // Just below the top wall.
            // Add water instead of dirt for rain.
            cell.addWater(0.8);
        }

        // Schedule next rain drop based on current rate.
        double intervalSeconds = 1.0 / config_.rain_rate;
        nextRainDrop = simTime + intervalSeconds;
    }

    // Water column refill (if enabled).
    if (config_.water_column_enabled) {
        // Initialize start time to actual simTime on first call (if still at initial value).
        if (waterColumnStartTime == 0.0) {
            waterColumnStartTime = simTime;
            spdlog::info(
                "Water column starting at time {:.3f}s (will auto-disable after {:.1f}s)",
                waterColumnStartTime,
                WATER_COLUMN_DURATION);
        }

        // Check if water column has been running for more than WATER_COLUMN_DURATION seconds.
        if (waterColumnStartTime > 0.0) {
            double elapsed = simTime - waterColumnStartTime;
            if (elapsed >= WATER_COLUMN_DURATION) {
                spdlog::info(
                    "Water column auto-disabling after {:.1f} seconds (elapsed: {:.1f}s)",
                    WATER_COLUMN_DURATION,
                    elapsed);
                config_.water_column_enabled = false;
                waterColumnAutoDisabled_ = true;
                waterColumnStartTime = -1.0;
            }
        }

        // Refill if still enabled.
        if (config_.water_column_enabled) {
            // Scale water column dimensions based on world size (same as waterColumnToggle).
            uint32_t columnWidth = std::max(3u, std::min(8u, world.getData().width / 20));
            uint32_t columnHeight = world.getData().height / 3;

            // Refill any empty or water cells in the water column area.
            for (uint32_t y = 0; y < columnHeight && y < world.getData().height; ++y) {
                for (uint32_t x = 1; x <= columnWidth && x < world.getData().width; ++x) {
                    Cell& cell = world.getData().at(x, y);
                    // Only refill if cell is air or water, and not already full.
                    if ((cell.material_type == MaterialType::AIR
                         || cell.material_type == MaterialType::WATER)
                        && !cell.isFull()) {
                        cell.addWater(1.0 - cell.fill_ratio);
                    }
                }
            }
        }
    }

    lastSimTime = simTime;
}

// Feature-preserving resize implementation.
std::vector<WorldEventGenerator::ResizeData> WorldEventGenerator::captureWorldState(
    const World& /* world. */) const
{
    // Resize functionality not available for WorldInterface - requires direct cell access.
    // Return empty state for now - TODO: implement resize support if needed.
    spdlog::warn("captureWorldState not implemented for WorldInterface - resize not supported");
    return {};
}

void WorldEventGenerator::applyWorldState(
    World& world,
    const std::vector<ResizeData>& oldState,
    uint32_t oldWidth,
    uint32_t oldHeight) const
{
    uint32_t newWidth = world.getData().width;
    uint32_t newHeight = world.getData().height;

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

double WorldEventGenerator::calculateEdgeStrength(
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

WorldEventGenerator::ResizeData WorldEventGenerator::interpolateCell(
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

WorldEventGenerator::ResizeData WorldEventGenerator::bilinearInterpolate(
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

WorldEventGenerator::ResizeData WorldEventGenerator::nearestNeighborSample(
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

// Water column timing and auto-disable support.
void ConfigurableWorldEventGenerator::setWaterColumnEnabled(bool enabled)
{
    config_.water_column_enabled = enabled;
    if (enabled) {
        // Record start time when enabled.
        waterColumnStartTime = lastSimTime;
        spdlog::info(
            "Water column enabled at time {:.3f}s (will auto-disable after {:.1f}s)",
            waterColumnStartTime,
            WATER_COLUMN_DURATION);
    }
    else {
        // Clear start time when disabled.
        waterColumnStartTime = -1.0;
    }
}

bool ConfigurableWorldEventGenerator::checkAndClearWaterColumnAutoDisabled()
{
    if (waterColumnAutoDisabled_) {
        waterColumnAutoDisabled_ = false;
        return true;
    }
    return false;
}

double ConfigurableWorldEventGenerator::getWaterColumnElapsedTime() const
{
    if (waterColumnStartTime < 0.0 || !config_.water_column_enabled) {
        return 0.0;
    }
    return lastSimTime - waterColumnStartTime;
}
