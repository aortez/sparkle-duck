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
#include "Vector2i.h"
#include "WorldFactory.h"
#include "WorldInterface.h"
#include "WorldSetup.h"
#include "WorldState.h"

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

class World : public WorldInterface {
public:
    World(uint32_t width, uint32_t height, lv_obj_t* draw_area);
    ~World();

    // World is not copyable due to unique_ptr members.
    World(const World&) = delete;
    World& operator=(const World&) = delete;

    // Default move constructor and move assignment operator.
    World(World&&) = default;
    World& operator=(World&&) = default;

    void advanceTime(double deltaTimeSeconds) override;

    uint32_t getTimestep() const override { return timestep; }

    void draw() override;

    void reset() override;

    Cell& at(uint32_t x, uint32_t y);
    const Cell& at(uint32_t x, uint32_t y) const;
    Cell& at(const Vector2i& pos);
    const Cell& at(const Vector2i& pos) const;

    // WorldInterface cell access through CellInterface
    CellInterface& getCellInterface(uint32_t x, uint32_t y) override;
    const CellInterface& getCellInterface(uint32_t x, uint32_t y) const override;

    uint32_t getWidth() const override;
    uint32_t getHeight() const override;
    lv_obj_t* getDrawArea() const override { return draw_area; }

    void setTimescale(double scale) override { timescale = scale; }
    double getTimescale() const override { return timescale; }

    // UI management
    void setUI(std::unique_ptr<SimulatorUI> ui) override;
    void setUIReference(SimulatorUI* ui) override;
    SimulatorUI* getUI() const override { return ui_ref_ ? ui_ref_ : ui_.get(); }

    // World type identification
    const char* getWorldTypeName() const override { return "World (RulesA)"; }

    // Get the total mass of dirt in the world.
    double getTotalMass() const override;

    // Get the amount of dirt that has been removed due to being below the threshold.
    double getRemovedMass() const override { return removedMass; }

    // Control whether addParticles should be called during advanceTime
    void setAddParticlesEnabled(bool enabled) override { addParticlesEnabled = enabled; }

    // Add dirt at a specific pixel coordinate
    void addDirtAtPixel(int pixelX, int pixelY) override;
    void addWaterAtPixel(int pixelX, int pixelY) override;

    // Universal material addition (mapped to dirt/water for WorldA compatibility)
    void addMaterialAtPixel(
        int pixelX, int pixelY, MaterialType type, double amount = 1.0) override;

    // Add material at cell coordinates (useful for testing)
    void addMaterialAtCell(uint32_t x, uint32_t y, MaterialType type, double amount = 1.0) override;

    // Material selection state management
    void setSelectedMaterial(MaterialType type) override { selected_material_ = type; }
    MaterialType getSelectedMaterial() const override { return selected_material_; }

    // Check if cell at pixel coordinates has material
    bool hasMaterialAtPixel(int pixelX, int pixelY) const override;

    // Time reversal functionality
    void enableTimeReversal(bool enabled) override { timeReversalEnabled = enabled; }
    bool isTimeReversalEnabled() const override { return timeReversalEnabled; }
    void saveWorldState() override;
    bool canGoBackward() const override { return !stateHistory.empty(); }
    bool canGoForward() const override
    {
        return currentHistoryIndex < static_cast<int>(stateHistory.size()) - 1;
    }
    void goBackward() override;
    void goForward() override;
    void clearHistory() override
    {
        stateHistory.clear();
        currentHistoryIndex = -1;
        hasStoredCurrentState = false; // Also clear stored current state
    }
    size_t getHistorySize() const override { return stateHistory.size(); }

    // Start dragging dirt from a cell
    void startDragging(int pixelX, int pixelY) override;

    // Update drag position
    void updateDrag(int pixelX, int pixelY) override;

    // End dragging and place dirt
    void endDragging(int pixelX, int pixelY) override;

    void restoreLastDragCell() override;

    void setGravity(double g) override { gravity = g; }

    // Set the elasticity factor for reflections
    void setElasticityFactor(double e) override { ELASTICITY_FACTOR = e; }

    // Set the pressure scale factor
    void setPressureScale(double scale) override { pressureScale = scale; }

    // Water physics configuration
    void setWaterPressureThreshold(double threshold) override
    {
        waterPressureThreshold = threshold;
    }
    double getWaterPressureThreshold() const override { return waterPressureThreshold; }

    // Resize the world grid based on new cell size
    void resizeGrid(uint32_t newWidth, uint32_t newHeight) override;

    // Cursor force interaction
    void setCursorForceEnabled(bool enabled) override { cursorForceEnabled = enabled; }
    void updateCursorForce(int pixelX, int pixelY, bool isActive) override;
    void clearCursorForce() override { cursorForceActive = false; }

    // Cohesion physics control (no-op for WorldA)
    void setCohesionEnabled([[maybe_unused]] bool enabled) override { /* no-op for WorldA */ }
    bool isCohesionEnabled() const override { return false; }

    // Cohesion force physics control (no-op for WorldA)
    void setCohesionForceEnabled([[maybe_unused]] bool enabled) override { /* no-op for WorldA */ }
    bool isCohesionForceEnabled() const override { return false; }

    // Cohesion strength parameters (no-op for WorldA)
    void setCohesionForceStrength([[maybe_unused]] double strength) override
    { /* no-op for WorldA */ }
    double getCohesionForceStrength() const override { return 1.0; }

    void setAdhesionStrength([[maybe_unused]] double strength) override { /* no-op for WorldA */ }
    double getAdhesionStrength() const override { return 1.0; }

    void setAdhesionEnabled([[maybe_unused]] bool enabled) override { /* no-op for WorldA */ }
    bool isAdhesionEnabled() const override { return false; }

    void setCohesionBindStrength([[maybe_unused]] double strength) override { /* no-op for WorldA */
    }
    double getCohesionBindStrength() const override { return 1.0; }

    // COM cohesion range control (no-op for WorldA)
    void setCOMCohesionRange([[maybe_unused]] uint32_t range) override { /* no-op for WorldA */ }
    uint32_t getCOMCohesionRange() const override { return 2; }

    // Dump timer statistics
    void dumpTimerStats() const override { timers.dumpTimerStats(); }

    // Minimum amount of matter that we should bother processing.
    static constexpr double MIN_MATTER_THRESHOLD = 0.001;

    // Physics constants
    static constexpr double COM_CELL_WIDTH = 2.0; // COM coordinate system width per cell
    static constexpr double REFLECTION_THRESHOLD =
        1.2;                                       // Trigger reflection at 1.2x normal threshold
    static constexpr double TRANSFER_FACTOR = 1.0; // Always transfer 100% of available space

    // Controls how much dirt is left behind during transfers.
    static double DIRT_FRAGMENTATION_FACTOR;

    void setDirtFragmentationFactor(double factor) override { DIRT_FRAGMENTATION_FACTOR = factor; }

    void applyPressure(const double timestep);

    // Update pressure for all cells based on COM deflection into neighbors
    void updateAllPressures(double timestep);

    // Alternative top-down pressure system with hydrostatic accumulation
    void updateAllPressuresTopDown(double timestep);

    // Iterative settling pressure system with multiple passes
    void updateAllPressuresIterativeSettling(double timestep);

    // Pressure system selection (use WorldInterface enum)
    using PressureSystem = WorldInterface::PressureSystem;

    void setPressureSystem(PressureSystem system) override { pressureSystem = system; }
    PressureSystem getPressureSystem() const override { return pressureSystem; }

    // Dual pressure system controls (no-op for World - only WorldB supports these)
    void setHydrostaticPressureEnabled(bool /* enabled */) override { /* no-op */ }
    bool isHydrostaticPressureEnabled() const override { return false; }

    void setDynamicPressureEnabled(bool /* enabled */) override { /* no-op */ }
    bool isDynamicPressureEnabled() const override { return false; }

    double timescale = 1.0;

    // Mark that user input has occurred (for triggering saves)
    void markUserInput() override { hasUserInputSinceLastSave = true; }

    // World type management
    WorldType getWorldType() const override;
    void preserveState(::WorldState& state) const override;
    void restoreState(const ::WorldState& state) override;

protected:
    Timers timers;

    // WorldInterface hook implementations
    void onPreResize(uint32_t /*newWidth*/, uint32_t /*newHeight*/) override;

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
    void handleTransferFailure(
        Cell& cell,
        uint32_t x,
        uint32_t y,
        int targetX,
        int targetY,
        bool shouldTransferX,
        bool shouldTransferY);
    void handleBoundaryReflection(
        Cell& cell, int targetX, int targetY, bool shouldTransferX, bool shouldTransferY);
    void checkExcessiveDeflectionReflection(Cell& cell);

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
    bool isWithinBounds(const Vector2i& pos) const;
    Vector2d calculateNaturalCOM(const Vector2d& sourceCOM, int deltaX, int deltaY);
    Vector2d clampCOMToDeadZone(const Vector2d& naturalCOM);

    lv_obj_t* draw_area;
    uint32_t width;
    uint32_t height;
    std::vector<Cell> cells;

    uint32_t timestep = 0;

    // Pressure scale factor (default 1.0)
    double pressureScale = 1.0;

    // Water physics configuration
    double waterPressureThreshold = 0.0004; // Default threshold for water pressure application
                                            // (further lowered for easier flow)

    // Cursor force state
    bool cursorForceEnabled = true;
    bool cursorForceActive = false;
    int cursorForceX = 0;
    int cursorForceY = 0;
    static constexpr double CURSOR_FORCE_STRENGTH = 10.0; // Adjust this to control force magnitude
    static constexpr double CURSOR_FORCE_RADIUS = 5.0; // Number of cells affected by cursor force
    static double ELASTICITY_FACTOR; // Energy preserved in reflections (0.0 to 1.0)

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

    // Material selection state (for UI coordination)
    MaterialType selected_material_;

    // UI interface
    std::unique_ptr<SimulatorUI> ui_; // Owned UI (legacy architecture)
    SimulatorUI* ui_ref_;             // Non-owning reference (SimulationManager architecture)

    std::vector<DirtMove> moves;

    // Time reversal state history
    struct WorldState {
        std::vector<Cell> cells;
        uint32_t width;      // Grid width when this state was saved
        uint32_t height;     // Grid height when this state was saved
        uint32_t cellWidth;  // Cell width when this state was saved
        uint32_t cellHeight; // Cell height when this state was saved
        uint32_t timestep;
        double totalMass;
        double removedMass;
        double timestamp; // Time when this state was captured (in seconds)
        // Optional: also store physics state like pendingDragEnd, etc.
    };

    bool timeReversalEnabled = true;      // Enable by default
    std::vector<WorldState> stateHistory; // History of world states
    int currentHistoryIndex = -1;         // Current position in history (-1 = most recent)
    static constexpr size_t MAX_HISTORY_SIZE = 1000; // Limit history to prevent memory issues

    // Enhanced save logic
    bool hasUserInputSinceLastSave = false;               // Track user input for triggering saves
    double lastSaveTime = 0.0;                            // Track when we last saved (in seconds)
    double simulationTime = 0.0;                          // Total simulation time elapsed
    static constexpr double PERIODIC_SAVE_INTERVAL = 0.5; // Save every 500ms

    // Current state preservation for time reversal
    WorldState currentLiveState;        // Stored current state when starting navigation
    bool hasStoredCurrentState = false; // Whether we've captured the current state

    size_t coordToIndex(uint32_t x, uint32_t y) const;

    // Apply all queued moves to the world.
    void applyMoves();

    // Helper to convert pixel coordinates to cell coordinates
    void pixelToCell(int pixelX, int pixelY, int& cellX, int& cellY) const;
    Vector2i pixelToCell(int pixelX, int pixelY) const;

    // Helper to get world setup for resize operations
    const WorldSetup* getWorldSetup() const { return worldSetup_.get(); }

    // Helper method to restore state with potential grid size changes
    void restoreWorldState(const WorldState& state);

    // Pressure system selection
    PressureSystem pressureSystem = PressureSystem::Original;
};
