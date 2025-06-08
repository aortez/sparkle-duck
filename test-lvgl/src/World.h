#pragma once
/**
 * \file
 * A grid-based physical simulation. Energy is approximately conserved.
 * Particles are affected by gravity, kimenatics, and generally behavior like
 * sand in an hourglass.
 *
 * Within each Cell, the COM (center of mass) moves within the [-1,1] bounds. When it is within
 * the cell's internal deadzone, the COM moves internally. When it moves outside the deadzone,
 * the cell's contents transfer to the neighboring cell.
 */

#include "Cell.h"
#include "Timers.h"
#include "WorldSetup.h"

#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

// Forward declarations
class SimulatorUI;

struct DirtMove {
    uint32_t fromX;
    uint32_t fromY;
    uint32_t toX;
    uint32_t toY;
    double dirtAmount;
    double waterAmount;
    Vector2d comOffset;
};

class World {
public:
    World(uint32_t width, uint32_t height, lv_obj_t* draw_area);
    ~World();

    // World is not copyable due to unique_ptr members
    World(const World&) = delete;
    World& operator=(const World&) = delete;

    // Default move constructor and move assignment operator
    World(World&&) = default;
    World& operator=(World&&) = default;

    void advanceTime(const double deltaTimeSeconds);

    void draw();

    void reset();

    Cell& at(uint32_t x, uint32_t y);
    const Cell& at(uint32_t x, uint32_t y) const;

    uint32_t getWidth() const;
    uint32_t getHeight() const;
    lv_obj_t* getDrawArea() const { return draw_area; }

    void setTimescale(double scale) { timescale = scale; }

    // UI management
    void setUI(std::unique_ptr<SimulatorUI> ui);
    SimulatorUI* getUI() const { return ui_.get(); }

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

    // Resize the world grid based on new cell size
    void resizeGrid(uint32_t newWidth, uint32_t newHeight);

    // Cursor force interaction
    void setCursorForceEnabled(bool enabled) { cursorForceEnabled = enabled; }
    void updateCursorForce(int pixelX, int pixelY, bool isActive);
    void clearCursorForce() { cursorForceActive = false; }

    // Dump timer statistics
    void dumpTimerStats() const { timers.dumpTimerStats(); }

    // Minimum amount of dirt before it's considered "empty" and removed.
    static constexpr double MIN_DIRT_THRESHOLD = 0.01;

    // Physics constants
    static constexpr double COM_CELL_WIDTH = 2.0; // COM coordinate system width per cell
    static constexpr double REFLECTION_THRESHOLD =
        1.2;                                       // Trigger reflection at 1.2x normal threshold
    static constexpr double TRANSFER_FACTOR = 1.0; // Always transfer 100% of available space

    // Controls how much dirt is left behind during transfers.
    static double DIRT_FRAGMENTATION_FACTOR;

    void setDirtFragmentationFactor(double factor) { DIRT_FRAGMENTATION_FACTOR = factor; }

    void applyPressure(const double timestep);

    // Update pressure for all cells based on COM deflection into neighbors
    void updateAllPressures(double timestep);

    // Set the world setup strategy
    void setWorldSetup(std::unique_ptr<WorldSetup> setup) { worldSetup = std::move(setup); }

    // Get the current world setup strategy
    std::unique_ptr<WorldSetup> getWorldSetup() { return std::move(worldSetup); }

    // ConfigurableWorldSetup control methods
    void setLeftThrowEnabled(bool enabled);
    void setRightThrowEnabled(bool enabled);
    void setLowerRightQuadrantEnabled(bool enabled);
    void setWallsEnabled(bool enabled);
    void setRainRate(double rate);
    bool isLeftThrowEnabled() const;
    bool isRightThrowEnabled() const;
    bool isLowerRightQuadrantEnabled() const;
    bool areWallsEnabled() const;
    double getRainRate() const;

    double timescale = 1.0;

protected:
    Timers timers;

private:
    // Physics simulation methods (broken down from advanceTime)
    void processParticleAddition(double deltaTimeSeconds);
    void processDragEnd();
    void applyPhysicsToCell(Cell& cell, uint32_t x, uint32_t y, double deltaTimeSeconds);
    void processTransfers(double deltaTimeSeconds);
    bool attemptTransfer(
        Cell& cell,
        uint32_t x,
        uint32_t y,
        int targetX,
        int targetY,
        const Vector2d& comOffset,
        double totalMass);
    void handleBoundaryReflection(
        Cell& cell, int targetX, int targetY, bool shouldTransferX, bool shouldTransferY);
    void checkExcessiveDeflectionReflection(Cell& cell);
    void updateTotalMass();

    // Transfer calculation helpers
    void calculateTransferDirection(
        const Cell& cell,
        bool& shouldTransferX,
        bool& shouldTransferY,
        int& targetX,
        int& targetY,
        Vector2d& comOffset,
        uint32_t x,
        uint32_t y);
    bool isWithinBounds(int x, int y) const;
    Vector2d calculateNaturalCOM(const Vector2d& sourceCOM, int deltaX, int deltaY);
    Vector2d clampCOMToDeadZone(const Vector2d& naturalCOM);

    lv_obj_t* draw_area;
    uint32_t width;
    uint32_t height;
    std::vector<Cell> cells;

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

    // UI interface
    std::unique_ptr<SimulatorUI> ui_;

    std::vector<DirtMove> moves;

    size_t coordToIndex(uint32_t x, uint32_t y) const;

    // Apply all queued moves to the world.
    void applyMoves();

    // Helper to convert pixel coordinates to cell coordinates
    void pixelToCell(int pixelX, int pixelY, int& cellX, int& cellY) const;
};
