#pragma once

#include "Cell.h"
#include "Timers.h"
#include "WorldSetup.h"

#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

/**
 * A grid-based physical simulation. Energy is approximately conserved.
 * Particles are affected by gravity, kimenatics, and generally behavior like
 * sand in an hourglass.
 *
 * Within each Cell, the COM bounces within the [-1,1] bounds. It transfers
 * to neighboring cells when space is available, otherwise reflecting internally.
 */
class World {
public:
    World(uint32_t width, uint32_t height, lv_obj_t* draw_area);
    ~World();

    // World is not copyable due to unique_ptr members
    World(const World&) = delete;
    World& operator=(const World&) = delete;

    void advanceTime(uint32_t deltaTimeMs);

    void draw();

    void reset();

    Cell& at(uint32_t x, uint32_t y);
    const Cell& at(uint32_t x, uint32_t y) const;

    uint32_t getWidth() const;
    uint32_t getHeight() const;
    lv_obj_t* getDrawArea() const { return draw_area; }

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
    void addWaterAtPixel(int pixelX, int pixelY);

    // Start dragging dirt from a cell
    void startDragging(int pixelX, int pixelY);

    // Update drag position
    void updateDrag(int pixelX, int pixelY);

    // End dragging and place dirt
    void endDragging(int pixelX, int pixelY);

    void restoreLastDragCell();

    void setGravity(double g) { gravity = g; }

    // Set the elasticity factor for reflections
    void setElasticityFactor(double e) { ELASTICITY_FACTOR = e; }

    // Set the pressure scale factor
    void setPressureScale(double scale) { pressureScale = scale; }

    // Cursor force interaction
    void setCursorForceEnabled(bool enabled) { cursorForceEnabled = enabled; }
    void updateCursorForce(int pixelX, int pixelY, bool isActive);
    void clearCursorForce() { cursorForceActive = false; }

    // Dump timer statistics
    void dumpTimerStats() const { timers.dumpTimerStats(); }

    // Minimum amount of dirt before it's considered "empty" and removed.
    static constexpr double MIN_DIRT_THRESHOLD = 0.01;

    // Controls how much dirt is left behind during transfers.
    static double DIRT_FRAGMENTATION_FACTOR;

    void setDirtFragmentationFactor(double factor) { DIRT_FRAGMENTATION_FACTOR = factor; }

    // Update pressure for all cells based on COM deflection into neighbors
    void updateAllPressures(double timestep);

    // Set the world setup strategy
    void setWorldSetup(std::unique_ptr<WorldSetup> setup) { worldSetup = std::move(setup); }

    // Get the current world setup strategy
    std::unique_ptr<WorldSetup> getWorldSetup() { return std::move(worldSetup); }

    // Control whether to fill the lower right quadrant during reset
    void setFillLowerRightQuadrant(bool enabled) { shouldFillLowerRightQuadrant = enabled; }

    // Fill the lower right quadrant with dirt
    void fillLowerRightQuadrant();

protected:
    Timers timers;

private:
    lv_obj_t* draw_area;
    uint32_t width;
    uint32_t height;
    std::vector<Cell> cells;
    double timescale = 1.0;
    uint32_t timestep = 0;

    // Pressure scale factor (default 1.0)
    double pressureScale = 1.0;

    // Cursor force state
    bool cursorForceEnabled = true;
    bool cursorForceActive = false;
    int cursorForceX = 0;
    int cursorForceY = 0;
    static constexpr double CURSOR_FORCE_STRENGTH = 10.0; // Adjust this to control force magnitude
    static constexpr double CURSOR_FORCE_RADIUS = 5.0; // Number of cells affected by cursor force
    static double ELASTICITY_FACTOR; // Energy preserved in reflections (0.0 to 1.0)

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
    int dragStartX = -1;
    int dragStartY = -1;
    double draggedDirt = 0.0;
    Vector2d draggedVelocity;
    Vector2d draggedCom; // Track COM during drag
    int lastDragCellX = -1;
    int lastDragCellY = -1;
    double lastCellOriginalDirt = 0.0;
    std::vector<std::pair<int, int>> recentPositions;
    static constexpr size_t MAX_RECENT_POSITIONS = 5;

    // Track pending drag end state
    struct PendingDragEnd {
        bool hasPendingEnd = false;
        int cellX = -1;
        int cellY = -1;
        double dirt = 0.0;
        Vector2d velocity;
        Vector2d com;
    };
    PendingDragEnd pendingDragEnd;

    // World setup strategy
    std::unique_ptr<WorldSetup> worldSetup;

    // Control whether to fill the lower right quadrant during reset
    bool shouldFillLowerRightQuadrant = true;

    size_t coordToIndex(uint32_t x, uint32_t y) const;

    // Helper to convert pixel coordinates to cell coordinates
    void pixelToCell(int pixelX, int pixelY, int& cellX, int& cellY) const;
};
