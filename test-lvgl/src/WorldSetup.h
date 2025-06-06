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
    virtual void addParticles(
        World& world, uint32_t timestep, double deltaTimeSeconds) = 0;

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

    void addParticles(
        World& world, uint32_t timestep, double deltaTimeSeconds) override;
};
