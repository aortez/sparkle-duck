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

    // Add dirt at a specific pixel coordinate
    void addDirtAtPixel(int pixelX, int pixelY);

    // Start dragging dirt from a cell
    void startDragging(int pixelX, int pixelY);
    
    // Update drag position
    void updateDrag(int pixelX, int pixelY);
    
    // End dragging and place dirt
    void endDragging(int pixelX, int pixelY);

    void restoreLastDragCell();

    void setGravity(double g) { gravity = g; }

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

    // Gravity (default 9.81, can be set for tests)
    double gravity = 9.81;

    // Drag state
    bool isDragging = false;
    int dragStartX = 0;
    int dragStartY = 0;
    double draggedDirt = 0.0;
    Vector2d draggedVelocity;
    int lastDragCellX = -1;
    int lastDragCellY = -1;
    double lastCellOriginalDirt = 0.0;
    std::vector<std::pair<int, int>> recentPositions;
    static const int MAX_RECENT_POSITIONS = 5;

    size_t coordToIndex(uint32_t x, uint32_t y) const;
    
    // Helper to convert pixel coordinates to cell coordinates
    void pixelToCell(int pixelX, int pixelY, int& cellX, int& cellY) const;
};
