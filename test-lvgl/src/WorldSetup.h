#pragma once

#include "Vector2d.h"
#include <cstdint>
#include <vector>

// Forward declarations
class WorldInterface;

/**
 * Interface for World setup strategies.
 * This allows different ways of setting up the World's initial state.
 */
class WorldSetup {
public:
    // Struct for storing cell data during resize operations
    struct ResizeData {
        double dirt;
        double water;
        Vector2d com;
        Vector2d velocity;
    };

    virtual ~WorldSetup() = default;

    // Setup the world's initial state.
    virtual void setup(WorldInterface& world) = 0;

    // Add particles to the world during simulation.
    virtual void addParticles(WorldInterface& world, uint32_t timestep, double deltaTimeSeconds) = 0;

    // Resize functionality - can be overridden by different strategies
    virtual std::vector<ResizeData> captureWorldState(const WorldInterface& world) const;
    virtual void applyWorldState(
        WorldInterface& world,
        const std::vector<ResizeData>& oldState,
        uint32_t oldWidth,
        uint32_t oldHeight) const;

    virtual void fillLowerRightQuadrant(WorldInterface& world);
    virtual void makeWalls(WorldInterface& world);
    virtual void fillWithDirt(WorldInterface& world);

protected:
    // Helper functions for feature-preserving resize
    virtual double calculateEdgeStrength(
        const std::vector<ResizeData>& state,
        uint32_t width,
        uint32_t height,
        uint32_t x,
        uint32_t y) const;
    virtual ResizeData interpolateCell(
        const std::vector<ResizeData>& oldState,
        uint32_t oldWidth,
        uint32_t oldHeight,
        double newX,
        double newY,
        double edgeStrength) const;
    virtual ResizeData bilinearInterpolate(
        const std::vector<ResizeData>& oldState,
        uint32_t oldWidth,
        uint32_t oldHeight,
        double x,
        double y) const;
    virtual ResizeData nearestNeighborSample(
        const std::vector<ResizeData>& oldState,
        uint32_t oldWidth,
        uint32_t oldHeight,
        double x,
        double y) const;
};

/**
 * Default implementation of WorldSetup that provides the standard setup behavior.
 */
class DefaultWorldSetup : public WorldSetup {
public:
    ~DefaultWorldSetup() override;

    void setup(WorldInterface& world) override;

    void addParticles(WorldInterface& world, uint32_t timestep, double deltaTimeSeconds) override;
};

/**
 * Configurable WorldSetup that allows toggling features on/off
 */
class ConfigurableWorldSetup : public WorldSetup {
public:
    ~ConfigurableWorldSetup() override = default;

    void setup(WorldInterface& world) override;
    void addParticles(WorldInterface& world, uint32_t timestep, double deltaTimeSeconds) override;

    // Control flags for setup features
    void setLowerRightQuadrantEnabled(bool enabled) { lowerRightQuadrantEnabled = enabled; }
    void setWallsEnabled(bool enabled) { wallsEnabled = enabled; }

    // Control flags for particle generation features
    void setLeftThrowEnabled(bool enabled) { leftThrowEnabled = enabled; }
    void setRightThrowEnabled(bool enabled) { rightThrowEnabled = enabled; }
    void setTopDropEnabled(bool enabled) { topDropEnabled = enabled; }
    void setSweepEnabled(bool enabled) { sweepEnabled = enabled; }
    void setRainRate(double rate) { rainRate = rate; }

    // Getters for current state
    bool isLowerRightQuadrantEnabled() const { return lowerRightQuadrantEnabled; }
    bool areWallsEnabled() const { return wallsEnabled; }
    bool isLeftThrowEnabled() const { return leftThrowEnabled; }
    bool isRightThrowEnabled() const { return rightThrowEnabled; }
    bool isTopDropEnabled() const { return topDropEnabled; }
    bool isSweepEnabled() const { return sweepEnabled; }
    double getRainRate() const { return rainRate; }

private:
    // Setup control flags
    bool lowerRightQuadrantEnabled = true;
    bool wallsEnabled = true;

    // Particle generation control flags
    bool leftThrowEnabled = true;
    bool rightThrowEnabled = true;
    bool topDropEnabled = true;
    bool sweepEnabled = false; // Currently disabled in DefaultWorldSetup
    double rainRate = 0.0;     // Rain rate in drops per second, 0 = disabled
};
