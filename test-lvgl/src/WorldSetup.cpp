#include "WorldSetup.h"
#include "Cell.h"
#include "Vector2d.h"
#include "World.h"
#include <cmath>
#include <cstdint>
#include <iostream>
#include <random>

// Debug logging specifically for particle events.
#ifdef LOG_PARTICLES
#define LOG_PARTICLES(x) std::cout << "[Particles] " << x << std::endl
#else
#define LOG_PARTICLES(x) ((void)0)
#endif

void WorldSetup::fillLowerRightQuadrant(World& world)
{
    for (uint32_t y = world.getHeight() / 2; y < world.getHeight(); ++y) {
        for (uint32_t x = world.getWidth() / 2; x < world.getWidth(); ++x) {
            world.at(x, y).update(1.0, Vector2d(0.0, 0.0), Vector2d(0.0, 0.0));
            world.at(x, y).markDirty();
        }
    }
}

void WorldSetup::makeWalls(World& world)
{
    // Top and bottom walls
    for (uint32_t x = 0; x < world.getWidth(); x++) {
        // world.at(x, 0).update(1.0, Vector2d(0.0, 0.0), Vector2d(0.0, 0.0));
        // world.at(x, 0).markDirty();
        world.at(x, world.getHeight() - 1).update(1.0, Vector2d(0.0, 0.0), Vector2d(0.0, 0.0));
        world.at(x, world.getHeight() - 1).markDirty();
    }
    // Left and right walls
    for (uint32_t y = 1; y < world.getHeight() - 1; y++) {
        world.at(0, y).update(1.0, Vector2d(0.0, 0.0), Vector2d(0.0, 0.0));
        world.at(0, y).markDirty();
        world.at(world.getWidth() - 1, y).update(1.0, Vector2d(0.0, 0.0), Vector2d(0.0, 0.0));
        world.at(world.getWidth() - 1, y).markDirty();
    }
}

void WorldSetup::fillWithDirt(World& world)
{
    for (uint32_t y = 0; y < world.getHeight(); y++) {
        for (uint32_t x = 0; x < world.getWidth(); x++) {
            world.at(x, y).update(0.5, Vector2d(0.0, 0.0), Vector2d(1, 0.0));
            world.at(x, y).markDirty();
        }
    }
}

void DefaultWorldSetup::setup(World& world)
{
    fillLowerRightQuadrant(world);
    makeWalls(world);
}

void DefaultWorldSetup::addParticles(World& world, uint32_t timestep, double deltaTimeSeconds)
{
    static double lastSimTime = 0.0;
    static struct EventState {
        double nextTopDrop = 0.33;       // First top drop at 0.33s
        double nextInitialThrow = 0.17;  // First throw at 0.17s
        double nextPeriodicThrow = 0.83; // First periodic throw at 0.83s
        double nextRightThrow = 1.0;     // First right throw at 1.0s
        double sweepTime = 0.0;          // Time tracking for sweep
        double beatTime = 0.0;           // Time tracking for beats
        bool initialThrowDone = false;   // Track if initial throw has happened
        bool topDropDone = false;        // Track if top drop has happened
    } eventState;

    const double simTime = lastSimTime + deltaTimeSeconds;

    LOG_PARTICLES(
        "Timestep " << timestep << ": simTime=" << simTime << ", lastSimTime=" << lastSimTime
                    << ", deltaTime=" << deltaTimeSeconds);

    // Constants for sweep behavior
    const double SWEEP_PERIOD = 2.0; // Time for one complete sweep (left to right and back)
    const double DIRT_PERIOD = 0.5;  // Period for dirt amount oscillation
    const double SWEEP_SPEED = 2.0;  // Speed of the sweep
    const double DIRT_AMPLITUDE =
        0.5; // Amplitude of dirt oscillation (0.5 means dirt varies from 0.5 to 1.0)
    const double BEAT_PERIOD = 0.5;  // Length of one beat in seconds
    const int BEATS_PER_PATTERN = 8; // Total beats in the pattern
    const int BEATS_ON = 2;          // Number of beats the emitter is on

    // Update beat time
    eventState.beatTime += deltaTimeSeconds;
    if (eventState.beatTime >= BEAT_PERIOD * BEATS_PER_PATTERN) {
        eventState.beatTime -= BEAT_PERIOD * BEATS_PER_PATTERN;
    }

    // Calculate current beat in the pattern (0 to BEATS_PER_PATTERN-1)
    int currentBeat = static_cast<int>(eventState.beatTime / BEAT_PERIOD);
    // bool isEmitterOn = currentBeat < BEATS_ON;
    bool isEmitterOn = false;

    // Only update sweep and emit particles if the emitter is on
    if (isEmitterOn) {
        // Update sweep time
        eventState.sweepTime += deltaTimeSeconds;

        // Calculate sweep position (x coordinate)
        double sweepPhase = (eventState.sweepTime / SWEEP_PERIOD) * 2.0 * M_PI;
        double sweepX = (std::sin(sweepPhase) + 1.0) * 0.5; // Maps to [0,1]
        uint32_t xPos =
            static_cast<uint32_t>(sweepX * (world.getWidth() - 2)) + 1; // Maps to [1,width-2]

        // Calculate dirt amount oscillation
        double dirtPhase = (eventState.sweepTime / DIRT_PERIOD) * 2.0 * M_PI;
        double dirtAmount =
            0.5 + DIRT_AMPLITUDE * std::sin(dirtPhase); // Oscillates between 0.5 and 1.0

        // Emit particle at current sweep position
        Cell& cell = world.at(xPos, 1); // 1 to be just below the top wall
        cell.update(dirtAmount, Vector2d(0.0, 0.0), Vector2d(0.0, 0.0));
        LOG_PARTICLES(
            "Sweep emitter at x=" << xPos << " with dirt=" << dirtAmount << " (beat " << currentBeat
                                  << ")");
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
        uint32_t centerY = world.getHeight() / 2 - 2;
        Cell& cell = world.at(world.getWidth() - 3, centerY); // Against the right wall.
        cell.update(1.0, Vector2d(0.0, 0.0), Vector2d(-10, -10));
        // Schedule next throw
        eventState.nextRightThrow += period;
    }

    lastSimTime = simTime;
}

DefaultWorldSetup::~DefaultWorldSetup()
{}

// ConfigurableWorldSetup implementation
void ConfigurableWorldSetup::setup(World& world)
{
    if (lowerRightQuadrantEnabled) {
        fillLowerRightQuadrant(world);
    }
    if (wallsEnabled) {
        makeWalls(world);
    }
}

void ConfigurableWorldSetup::addParticles(World& world, uint32_t timestep, double deltaTimeSeconds)
{
    static double lastSimTime = 0.0;
    static struct EventState {
        double nextTopDrop = 0.33;       // First top drop at 0.33s
        double nextInitialThrow = 0.17;  // First throw at 0.17s
        double nextPeriodicThrow = 0.83; // First periodic throw at 0.83s
        double nextRightThrow = 1.0;     // First right throw at 1.0s
        double sweepTime = 0.0;          // Time tracking for sweep
        double beatTime = 0.0;           // Time tracking for beats
        bool initialThrowDone = false;   // Track if initial throw has happened
        bool topDropDone = false;        // Track if top drop has happened
        double nextRainDrop = 0.0;       // Time for next rain drop
    } eventState;

    const double simTime = lastSimTime + deltaTimeSeconds;

    LOG_PARTICLES(
        "Timestep " << timestep << ": simTime=" << simTime << ", lastSimTime=" << lastSimTime
                    << ", deltaTime=" << deltaTimeSeconds);

    // Constants for sweep behavior
    const double SWEEP_PERIOD = 2.0; // Time for one complete sweep (left to right and back)
    const double DIRT_PERIOD = 0.5;  // Period for dirt amount oscillation
    const double SWEEP_SPEED = 2.0;  // Speed of the sweep
    const double DIRT_AMPLITUDE =
        0.5; // Amplitude of dirt oscillation (0.5 means dirt varies from 0.5 to 1.0)
    const double BEAT_PERIOD = 0.5;  // Length of one beat in seconds
    const int BEATS_PER_PATTERN = 8; // Total beats in the pattern
    const int BEATS_ON = 2;          // Number of beats the emitter is on

    // Update beat time
    eventState.beatTime += deltaTimeSeconds;
    if (eventState.beatTime >= BEAT_PERIOD * BEATS_PER_PATTERN) {
        eventState.beatTime -= BEAT_PERIOD * BEATS_PER_PATTERN;
    }

    // Calculate current beat in the pattern (0 to BEATS_PER_PATTERN-1)
    int currentBeat = static_cast<int>(eventState.beatTime / BEAT_PERIOD);
    // bool isEmitterOn = currentBeat < BEATS_ON;
    bool isEmitterOn = false;

    // Only update sweep and emit particles if the emitter is on and sweep is enabled
    if (isEmitterOn && sweepEnabled) {
        // Update sweep time
        eventState.sweepTime += deltaTimeSeconds;

        // Calculate sweep position (x coordinate)
        double sweepPhase = (eventState.sweepTime / SWEEP_PERIOD) * 2.0 * M_PI;
        double sweepX = (std::sin(sweepPhase) + 1.0) * 0.5; // Maps to [0,1]
        uint32_t xPos =
            static_cast<uint32_t>(sweepX * (world.getWidth() - 2)) + 1; // Maps to [1,width-2]

        // Calculate dirt amount oscillation
        double dirtPhase = (eventState.sweepTime / DIRT_PERIOD) * 2.0 * M_PI;
        double dirtAmount =
            0.5 + DIRT_AMPLITUDE * std::sin(dirtPhase); // Oscillates between 0.5 and 1.0

        // Emit particle at current sweep position
        Cell& cell = world.at(xPos, 1); // 1 to be just below the top wall
        cell.update(dirtAmount, Vector2d(0.0, 0.0), Vector2d(0.0, 0.0));
        LOG_PARTICLES(
            "Sweep emitter at x=" << xPos << " with dirt=" << dirtAmount << " (beat " << currentBeat
                                  << ")");
    }

    // Drop a dirt from the top (if enabled)
    if (topDropEnabled && !eventState.topDropDone && simTime >= eventState.nextTopDrop) {
        LOG_PARTICLES("Adding top drop at time " << simTime);
        uint32_t centerX = world.getWidth() / 2;
        Cell& cell = world.at(centerX, 1); // 1 to be just below the top wall.
        cell.update(1.0, Vector2d(0.0, 0.0), Vector2d(0.0, 0.0));
        eventState.topDropDone = true;
    }

    // Initial throw from left center (if enabled)
    if (leftThrowEnabled && !eventState.initialThrowDone
        && simTime >= eventState.nextInitialThrow) {
        LOG_PARTICLES("Adding initial throw at time " << simTime);
        uint32_t centerY = world.getHeight() / 2;
        Cell& cell = world.at(2, centerY); // Against the left wall.
        cell.update(1.0, Vector2d(0.0, 0.0), Vector2d(5, -5));
        eventState.initialThrowDone = true;
    }

    // Recurring throws every ~0.83 seconds (if left throw enabled)
    const double period = 0.83;
    if (leftThrowEnabled && simTime >= eventState.nextPeriodicThrow) {
        LOG_PARTICLES("Adding periodic throw at time " << simTime);
        uint32_t centerY = world.getHeight() / 2;
        Cell& cell = world.at(2, centerY); // Against the left wall.
        cell.update(1.0, Vector2d(0.0, 0.0), Vector2d(10, -10));
        // Schedule next throw
        eventState.nextPeriodicThrow += period;
    }

    // Recurring throws from right side every ~0.83 seconds (if right throw enabled)
    if (rightThrowEnabled && simTime >= eventState.nextRightThrow) {
        LOG_PARTICLES("Adding right periodic throw at time " << simTime);
        uint32_t centerY = world.getHeight() / 2 - 2;
        Cell& cell = world.at(world.getWidth() - 3, centerY); // Against the right wall.
        cell.update(1.0, Vector2d(0.0, 0.0), Vector2d(-10, -10));
        // Schedule next throw
        eventState.nextRightThrow += period;
    }

    // Rain drops at variable rate (if rain rate > 0)
    if (rainRate > 0.0 && simTime >= eventState.nextRainDrop) {
        LOG_PARTICLES("Adding rain drop at time " << simTime << " (rate: " << rainRate << "/s)");

        // Use normal distribution for horizontal position
        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::normal_distribution<double> normalDist(0.5, 0.15); // mean=0.5, stddev=0.15

        // Generate random position across the top, clamped to world bounds
        double randomPos = std::max(0.0, std::min(1.0, normalDist(gen)));
        uint32_t xPos =
            static_cast<uint32_t>(randomPos * (world.getWidth() - 2)) + 1; // [1, width-2]

        Cell& cell = world.at(xPos, 1); // Just below the top wall
        // Add water instead of dirt for rain
        cell.water = std::min(1.0, cell.water + 0.8); // Add water, don't exceed 1.0
        cell.markDirty();

        // Schedule next rain drop based on current rate
        double intervalSeconds = 1.0 / rainRate;
        eventState.nextRainDrop = simTime + intervalSeconds;
    }

    lastSimTime = simTime;
}
