#pragma once

#include "Vector2d.h"
#include <cstdint>
#include <vector>

namespace DirtSim {

// Forward declarations
class World;

/**
 * Interface for World event generation strategies.
 * Handles initial world setup and dynamic particle generation during simulation.
 */
class WorldEventGenerator {
public:
    // Struct for storing cell data during resize operations
    struct ResizeData {
        double dirt;
        double water;
        Vector2d com;
        Vector2d velocity;
    };

    virtual ~WorldEventGenerator() = default;

    /**
     * @brief Clone this event generator for polymorphic copying.
     * @return A deep copy of this event generator.
     */
    virtual std::unique_ptr<WorldEventGenerator> clone() const = 0;

    // Clear the world to empty state (all cells reset).
    virtual void clear(World& world) = 0;

    // Setup the world's initial state.
    virtual void setup(World& world) = 0;

    // Add particles to the world during simulation.
    virtual void addParticles(World& world, uint32_t timestep, double deltaTimeSeconds) = 0;

    // Resize functionality - can be overridden by different strategies
    virtual std::vector<ResizeData> captureWorldState(const World& world) const;
    virtual void applyWorldState(
        World& world,
        const std::vector<ResizeData>& oldState,
        uint32_t oldWidth,
        uint32_t oldHeight) const;

    virtual void fillLowerRightQuadrant(World& world);
    virtual void makeWalls(World& world);
    virtual void makeMiddleMetalWall(World& world);
    virtual void fillWithDirt(World& world);

    // Toggle methods for add/remove features.
    virtual void dirtQuadrantToggle(World& world, bool enabled);
    virtual void waterColumnToggle(World& world, bool enabled);

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
 * Default implementation of WorldEventGenerator that provides standard setup behavior.
 */
class DefaultWorldEventGenerator : public WorldEventGenerator {
public:
    ~DefaultWorldEventGenerator();

    std::unique_ptr<WorldEventGenerator> clone() const;
    void clear(World& world) override;
    void setup(World& world) override;
    void addParticles(World& world, uint32_t timestep, double deltaTimeSeconds) override;

    // Event generation state.
    double lastSimTime = 0.0;
    double nextInitialThrow = 0.17;
    double nextPeriodicThrow = 0.83;
    double nextRightThrow = 1.0;
    bool initialThrowDone = false;
};

/**
 * Configurable WorldEventGenerator that allows toggling features on/off.
 */
class ConfigurableWorldEventGenerator : public WorldEventGenerator {
public:
    ~ConfigurableWorldEventGenerator() override = default;

    std::unique_ptr<WorldEventGenerator> clone() const override;
    void clear(World& world) override;
    void setup(World& world) override;
    void addParticles(World& world, uint32_t timestep, double deltaTimeSeconds) override;

    // Control flags for setup features
    void setLowerRightQuadrantEnabled(bool enabled) { lowerRightQuadrantEnabled = enabled; }
    void setWallsEnabled(bool enabled) { wallsEnabled = enabled; }
    void setMiddleMetalWallEnabled(bool enabled) { middleMetalWallEnabled = enabled; }

    // Control flags for particle generation features
    void setLeftThrowEnabled(bool enabled) { leftThrowEnabled = enabled; }
    void setRightThrowEnabled(bool enabled) { rightThrowEnabled = enabled; }
    void setSweepEnabled(bool enabled) { sweepEnabled = enabled; }
    void setRainRate(double rate) { rainRate = rate; }
    void setWaterColumnEnabled(bool enabled) { waterColumnEnabled = enabled; }

    // Getters for current state
    bool isLowerRightQuadrantEnabled() const { return lowerRightQuadrantEnabled; }
    bool areWallsEnabled() const { return wallsEnabled; }
    bool isMiddleMetalWallEnabled() const { return middleMetalWallEnabled; }
    bool isLeftThrowEnabled() const { return leftThrowEnabled; }
    bool isRightThrowEnabled() const { return rightThrowEnabled; }
    bool isSweepEnabled() const { return sweepEnabled; }
    double getRainRate() const { return rainRate; }
    bool isWaterColumnEnabled() const { return waterColumnEnabled; }

private:
    // Setup control flags
    bool lowerRightQuadrantEnabled = true;
    bool wallsEnabled = true;
    bool middleMetalWallEnabled = true;

    // Particle generation control flags.
    bool leftThrowEnabled = true;
    bool rightThrowEnabled = true;
    bool sweepEnabled = false;       // Currently disabled.
    double rainRate = 0.0;           // Rain rate in drops per second, 0 = disabled.
    bool waterColumnEnabled = false; // Water column on left side (5 wide × 20 tall).

    // Event generation state (moved from static variables).
    double lastSimTime = 0.0;
    double nextInitialThrow = 0.17;
    double nextPeriodicThrow = 0.83;
    double nextRightThrow = 1.0;
    double nextRainDrop = 0.0;
    bool initialThrowDone = false;
};

} // namespace DirtSim
