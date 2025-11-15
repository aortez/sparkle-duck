#pragma once

#include "ScenarioConfig.h"
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
 * This generator owns the authoritative SandboxConfig - it is the single source of truth.
 */
class ConfigurableWorldEventGenerator : public WorldEventGenerator {
private:
    // Authoritative config (single source of truth) - declared first for inline method access.
    SandboxConfig config_;

    // Setup control flags (not in SandboxConfig).
    bool wallsEnabled = true;
    bool middleMetalWallEnabled = true;

    // Particle generation control flags (not in SandboxConfig).
    bool leftThrowEnabled = true;
    bool sweepEnabled = false; // Currently disabled.

    // Event generation state (moved from static variables).
    double lastSimTime = 0.0;
    double nextInitialThrow = 0.17;
    double nextPeriodicThrow = 0.83;
    double nextRightThrow = 1.0;
    double nextRainDrop = 0.0;
    bool initialThrowDone = false;

    // Water column auto-disable state.
    double waterColumnStartTime = -1.0;    // Time when water column was enabled (-1 = not active).
    bool waterColumnAutoDisabled_ = false; // Flag: water column was auto-disabled.
    static constexpr double WATER_COLUMN_DURATION = 5.0; // Seconds to run before auto-disable.

public:
    // Constructor takes initial config (generator owns it).
    explicit ConfigurableWorldEventGenerator(const SandboxConfig& config);

    ~ConfigurableWorldEventGenerator() override = default;

    std::unique_ptr<WorldEventGenerator> clone() const override;
    void clear(World& world) override;
    void setup(World& world) override;
    void addParticles(World& world, uint32_t timestep, double deltaTimeSeconds) override;

    // Config access (generator is source of truth).
    const SandboxConfig& getConfig() const { return config_; }
    void updateConfig(const SandboxConfig& newConfig);

    // Control flags for setup features (these modify config_).
    void setLowerRightQuadrantEnabled(bool enabled) { config_.quadrant_enabled = enabled; }
    void setWallsEnabled(bool enabled) { wallsEnabled = enabled; }
    void setMiddleMetalWallEnabled(bool enabled) { middleMetalWallEnabled = enabled; }

    // Control flags for particle generation features (these modify config_).
    void setLeftThrowEnabled(bool enabled) { leftThrowEnabled = enabled; }
    void setRightThrowEnabled(bool enabled) { config_.right_throw_enabled = enabled; }
    void setSweepEnabled(bool enabled) { sweepEnabled = enabled; }
    void setRainRate(double rate) { config_.rain_rate = rate; }
    void setWaterColumnEnabled(bool enabled);

    // Getters for current state (read from config_).
    bool isLowerRightQuadrantEnabled() const { return config_.quadrant_enabled; }
    bool areWallsEnabled() const { return wallsEnabled; }
    bool isMiddleMetalWallEnabled() const { return middleMetalWallEnabled; }
    bool isLeftThrowEnabled() const { return leftThrowEnabled; }
    bool isRightThrowEnabled() const { return config_.right_throw_enabled; }
    bool isSweepEnabled() const { return sweepEnabled; }
    double getRainRate() const { return config_.rain_rate; }
    bool isWaterColumnEnabled() const { return config_.water_column_enabled; }

    // Check if water column was auto-disabled (returns true once, then clears flag).
    bool checkAndClearWaterColumnAutoDisabled();

    // Get elapsed time since water column was enabled (for UI feedback).
    double getWaterColumnElapsedTime() const;
};

} // namespace DirtSim
