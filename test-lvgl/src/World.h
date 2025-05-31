#pragma once

#include "Cell.h"

#include <cstdint>
#include <vector>

class World {
public:
    World(uint32_t width, uint32_t height, lv_obj_t* draw_area);

    void advanceTime(uint32_t deltaTimeMs);

    void draw();

    void reset();

    Cell& at(uint32_t x, uint32_t y);
    const Cell& at(uint32_t x, uint32_t y) const;

    uint32_t getWidth() const;
    uint32_t getHeight() const;

    void fillWithDirt();
    void makeWalls();

    void setTimescale(double scale) { timescale = scale; }

    // Get the total mass of dirt in the world.
    double getTotalMass() const { return totalMass; }

    // Get the amount of dirt that has been removed due to being below the threshold.
    double getRemovedMass() const { return removedMass; }

    // Control whether addParticles should be called during advanceTime
    void setAddParticlesEnabled(bool enabled) { addParticlesEnabled = enabled; }

private:
    lv_obj_t* draw_area;
    uint32_t width;
    uint32_t height;
    std::vector<Cell> cells;
    double timescale = 1.0;
    uint32_t timestep = 0;

    // Minimum amount of dirt before it's considered "empty" and removed.
    static constexpr double MIN_DIRT_THRESHOLD = 0.001;

    // Track the total mass of dirt in the world.
    double totalMass = 0.0;

    // Track mass that has been removed due to being below the threshold.
    double removedMass = 0.0;

    // Control whether addParticles should be called during advanceTime
    bool addParticlesEnabled = true;

    size_t coordToIndex(uint32_t x, uint32_t y) const;
};
