#pragma once

#include "CellB.h"
#include "MaterialType.h"
#include "Timers.h"
#include "Vector2i.h"
#include "WorldInterface.h"
#include "WorldSetup.h"
#include "WorldState.h"
#include "WorldFactory.h"

#include <cstdint>
#include <memory>
#include <vector>

// Forward declarations
class SimulatorUI;

/**
 * \file
 * WorldB implements the pure-material physics system based on GridMechanics.md.
 * Unlike World (mixed dirt/water), WorldB uses CellB with pure materials and
 * fill ratios, providing a simpler but different physics model.
 */

class WorldB : public WorldInterface {
public:
    WorldB(uint32_t width, uint32_t height, lv_obj_t* draw_area);
    ~WorldB();

    // WorldB is not copyable due to unique_ptr members
    WorldB(const WorldB&) = delete;
    WorldB& operator=(const WorldB&) = delete;

    // Default move constructor and move assignment operator
    WorldB(WorldB&&) = default;
    WorldB& operator=(WorldB&&) = default;

    // =================================================================
    // WORLDINTERFACE IMPLEMENTATION - CORE SIMULATION
    // =================================================================
    
    void advanceTime(double deltaTimeSeconds) override;
    uint32_t getTimestep() const override { return timestep_; }
    void draw() override;
    void reset() override;
    
    // =================================================================
    // WORLDINTERFACE IMPLEMENTATION - GRID ACCESS
    // =================================================================
    
    uint32_t getWidth() const override { return width_; }
    uint32_t getHeight() const override { return height_; }
    lv_obj_t* getDrawArea() const override { return draw_area_; }
    
    // WorldInterface cell access through CellInterface
    CellInterface& getCellInterface(uint32_t x, uint32_t y) override;
    const CellInterface& getCellInterface(uint32_t x, uint32_t y) const override;
    
    // =================================================================
    // WORLDINTERFACE IMPLEMENTATION - SIMULATION CONTROL
    // =================================================================
    
    void setTimescale(double scale) override { timescale_ = scale; }
    double getTimescale() const override { return timescale_; }
    double getTotalMass() const override;
    double getRemovedMass() const override { return removed_mass_; }
    void setAddParticlesEnabled(bool enabled) override { add_particles_enabled_ = enabled; }
    
    // =================================================================
    // WORLDINTERFACE IMPLEMENTATION - MATERIAL ADDITION
    // =================================================================
    
    void addDirtAtPixel(int pixelX, int pixelY) override;
    void addWaterAtPixel(int pixelX, int pixelY) override;
    
    // Universal material addition (direct support for all 8 material types)
    void addMaterialAtPixel(int pixelX, int pixelY, MaterialType type, double amount = 1.0) override;
    
    // Material selection state management
    void setSelectedMaterial(MaterialType type) override { selected_material_ = type; }
    MaterialType getSelectedMaterial() const override { return selected_material_; }
    
    // =================================================================
    // WORLDINTERFACE IMPLEMENTATION - DRAG INTERACTION
    // =================================================================
    
    void startDragging(int pixelX, int pixelY) override;
    void updateDrag(int pixelX, int pixelY) override;
    void endDragging(int pixelX, int pixelY) override;
    void restoreLastDragCell() override;
    
    // =================================================================
    // WORLDINTERFACE IMPLEMENTATION - PHYSICS PARAMETERS
    // =================================================================
    
    void setGravity(double g) override { gravity_ = g; }
    void setElasticityFactor(double e) override { elasticity_factor_ = e; }
    void setPressureScale(double scale) override { pressure_scale_ = scale; }
    void setDirtFragmentationFactor(double factor) override { /* no-op for WorldB */ }
    
    // =================================================================
    // WORLDINTERFACE IMPLEMENTATION - WATER PHYSICS (SIMPLIFIED)
    // =================================================================
    
    void setWaterPressureThreshold(double threshold) override { water_pressure_threshold_ = threshold; }
    double getWaterPressureThreshold() const override { return water_pressure_threshold_; }
    
    // =================================================================
    // WORLDINTERFACE IMPLEMENTATION - PRESSURE SYSTEM
    // =================================================================
    
    void setPressureSystem(PressureSystem system) override { pressure_system_ = system; }
    PressureSystem getPressureSystem() const override { return pressure_system_; }
    
    // =================================================================
    // WORLDINTERFACE IMPLEMENTATION - TIME REVERSAL (NO-OP)
    // =================================================================
    
    void enableTimeReversal(bool enabled) override { /* no-op */ }
    bool isTimeReversalEnabled() const override { return false; }
    void saveWorldState() override { /* no-op */ }
    bool canGoBackward() const override { return false; }
    bool canGoForward() const override { return false; }
    void goBackward() override { /* no-op */ }
    void goForward() override { /* no-op */ }
    void clearHistory() override { /* no-op */ }
    size_t getHistorySize() const override { return 0; }
    
    // =================================================================
    // WORLDINTERFACE IMPLEMENTATION - WORLD SETUP (SIMPLIFIED)
    // =================================================================
    
    void setLeftThrowEnabled(bool enabled) override;
    void setRightThrowEnabled(bool enabled) override;
    void setLowerRightQuadrantEnabled(bool enabled) override;
    void setWallsEnabled(bool enabled) override;
    void setRainRate(double rate) override;
    bool isLeftThrowEnabled() const override;
    bool isRightThrowEnabled() const override;
    bool isLowerRightQuadrantEnabled() const override;
    bool areWallsEnabled() const override;
    double getRainRate() const override;
    
    // =================================================================
    // WORLDINTERFACE IMPLEMENTATION - CURSOR FORCE
    // =================================================================
    
    void setCursorForceEnabled(bool enabled) override { cursor_force_enabled_ = enabled; }
    void updateCursorForce(int pixelX, int pixelY, bool isActive) override;
    void clearCursorForce() override { cursor_force_active_ = false; }
    
    // =================================================================
    // WORLDINTERFACE IMPLEMENTATION - GRID MANAGEMENT
    // =================================================================
    
    void resizeGrid(uint32_t newWidth, uint32_t newHeight, bool clearHistory = true) override;
    
    // =================================================================
    // WORLDINTERFACE IMPLEMENTATION - PERFORMANCE AND DEBUGGING
    // =================================================================
    
    void dumpTimerStats() const override { timers_.dumpTimerStats(); }
    void markUserInput() override { /* no-op for now */ }
    
    // =================================================================
    // WORLDINTERFACE IMPLEMENTATION - WORLD TYPE MANAGEMENT
    // =================================================================
    
    WorldType getWorldType() const override;
    void preserveState(::WorldState& state) const override;
    void restoreState(const ::WorldState& state) override;
    
    // =================================================================
    // WORLDINTERFACE IMPLEMENTATION - UI INTEGRATION
    // =================================================================
    
    void setUI(std::unique_ptr<SimulatorUI> ui) override;
    void setUIReference(SimulatorUI* ui) override;
    SimulatorUI* getUI() const override { return ui_ref_ ? ui_ref_ : ui_.get(); }
    
    // =================================================================
    // WORLDB-SPECIFIC METHODS
    // =================================================================
    
    // Direct cell access
    CellB& at(uint32_t x, uint32_t y);
    const CellB& at(uint32_t x, uint32_t y) const;
    CellB& at(const Vector2i& pos);
    const CellB& at(const Vector2i& pos) const;
    
    // Add material at specific cell coordinates
    void addMaterialAtCell(uint32_t x, uint32_t y, MaterialType type, double amount = 1.0);
    
    // Physics constants from GridMechanics.md
    static constexpr double MAX_VELOCITY = 0.9;           // cells/timestep
    static constexpr double VELOCITY_DAMPING_THRESHOLD = 0.5;  // velocity threshold for damping
    static constexpr double VELOCITY_DAMPING_FACTOR = 0.10;    // 10% slowdown
    static constexpr double MIN_MATTER_THRESHOLD = 0.001;      // minimum matter to process

private:
    // =================================================================
    // INTERNAL PHYSICS METHODS
    // =================================================================
    
    // Physics simulation steps
    void applyGravity(double deltaTime);
    void updateTransfers(double deltaTime);
    void applyPressure(double deltaTime);
    void processVelocityLimiting();
    
    // Material transfer system
    struct MaterialMove {
        uint32_t fromX, fromY;
        uint32_t toX, toY;
        double amount;
        MaterialType material;
        Vector2d momentum;
    };
    
    void queueMaterialMoves(double deltaTime);
    void processMaterialMoves();
    
    // Floating particle collision detection
    bool checkFloatingParticleCollision(int cellX, int cellY);
    void handleFloatingParticleCollision(int cellX, int cellY);
    
    // Pressure calculation (simplified hydrostatic)
    void calculateHydrostaticPressure();
    
    // Boundary wall management
    void setupBoundaryWalls();
    
    // Coordinate conversion helpers
    void pixelToCell(int pixelX, int pixelY, int& cellX, int& cellY) const;
    Vector2i pixelToCell(int pixelX, int pixelY) const;
    bool isValidCell(int x, int y) const;
    bool isValidCell(const Vector2i& pos) const;
    size_t coordToIndex(uint32_t x, uint32_t y) const;
    size_t coordToIndex(const Vector2i& pos) const;
    
    // =================================================================
    // MEMBER VARIABLES
    // =================================================================
    
    // Grid storage
    std::vector<CellB> cells_;
    uint32_t width_;
    uint32_t height_;
    lv_obj_t* draw_area_;
    
    // Simulation state
    uint32_t timestep_;
    double timescale_;
    double removed_mass_;
    
    // Physics parameters
    double gravity_;
    double elasticity_factor_;
    double pressure_scale_;
    double water_pressure_threshold_;
    PressureSystem pressure_system_;
    
    // World setup controls
    bool add_particles_enabled_;
    bool left_throw_enabled_;
    bool right_throw_enabled_;
    bool quadrant_enabled_;
    bool walls_enabled_;
    double rain_rate_;
    
    // Cursor force state
    bool cursor_force_enabled_;
    bool cursor_force_active_;
    int cursor_force_x_;
    int cursor_force_y_;
    
    // Drag state (enhanced with visual feedback)
    bool is_dragging_;
    int drag_start_x_;
    int drag_start_y_;
    MaterialType dragged_material_;
    double dragged_amount_;
    
    // Current drag position tracking
    int last_drag_cell_x_;
    int last_drag_cell_y_;
    
    // Floating particle for drag interaction (can collide with world)
    bool has_floating_particle_;
    CellB floating_particle_;
    double floating_particle_pixel_x_;
    double floating_particle_pixel_y_;
    
    // Velocity tracking for "toss" behavior
    std::vector<std::pair<int, int>> recent_positions_;
    Vector2d dragged_velocity_;
    Vector2d dragged_com_;
    
    // Material selection state (for UI coordination)
    MaterialType selected_material_;
    
    // Material transfer queue
    std::vector<MaterialMove> pending_moves_;
    
    // Performance timing
    mutable Timers timers_;
    
    // World setup strategy
    std::unique_ptr<WorldSetup> world_setup_;
    
    // UI interface
    std::unique_ptr<SimulatorUI> ui_;                    // Owned UI (legacy architecture)
    SimulatorUI* ui_ref_;                                // Non-owning reference (SimulationManager architecture)
};