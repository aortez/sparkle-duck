#pragma once

#include <cstdint>
// Forward declaration
class World;

/**
 * Interface for World setup strategies.
 * This allows different ways of setting up the World's initial state.
 */
class WorldSetup {
public:
    virtual ~WorldSetup() = default;

    // Setup the world's initial state.
    virtual void setup(World& world) = 0;

    // Add particles to the world during simulation.
    virtual void addParticles(World& world, uint32_t timestep, double deltaTimeSeconds) = 0;

    virtual void fillLowerRightQuadrant(World& world);
    virtual void makeWalls(World& world);
    virtual void fillWithDirt(World& world);
};

/**
 * Default implementation of WorldSetup that provides the standard setup behavior.
 */
class DefaultWorldSetup : public WorldSetup {
public:
    ~DefaultWorldSetup() override;

    void setup(World& world) override;

    void addParticles(World& world, uint32_t timestep, double deltaTimeSeconds) override;
};

/**
 * Configurable WorldSetup that allows toggling features on/off
 */
class ConfigurableWorldSetup : public WorldSetup {
public:
    ~ConfigurableWorldSetup() override = default;

    void setup(World& world) override;
    void addParticles(World& world, uint32_t timestep, double deltaTimeSeconds) override;

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
