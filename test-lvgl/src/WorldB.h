#pragma once

#include "CellB.h"
#include "MaterialMove.h"
#include "MaterialType.h"
#include "Timers.h"
#include "Vector2i.h"
#include "WorldBAdhesionCalculator.h"
#include "WorldBCohesionCalculator.h"
#include "WorldBCollisionCalculator.h"
#include "WorldBPressureCalculator.h"
#include "WorldBSupportCalculator.h"
#include "WorldFactory.h"
#include "WorldInterface.h"
#include "WorldSetup.h"
#include "WorldState.h"

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
    void setup() override;

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
    void addMaterialAtPixel(
        int pixelX, int pixelY, MaterialType type, double amount = 1.0) override;

    // Material selection state management
    void setSelectedMaterial(MaterialType type) override { selected_material_ = type; }
    MaterialType getSelectedMaterial() const override { return selected_material_; }

    // Check if cell at pixel coordinates has material
    bool hasMaterialAtPixel(int pixelX, int pixelY) const override;

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
    double getGravity() const { return gravity_; }
    void setElasticityFactor(double e) override { elasticity_factor_ = e; }
    void setPressureScale(double scale) override { pressure_scale_ = scale; }
    void setDirtFragmentationFactor(double /* factor */) override { /* no-op for WorldB */ }

    // =================================================================
    // WORLDINTERFACE IMPLEMENTATION - WATER PHYSICS (SIMPLIFIED)
    // =================================================================

    void setWaterPressureThreshold(double threshold) override
    {
        water_pressure_threshold_ = threshold;
    }
    double getWaterPressureThreshold() const override { return water_pressure_threshold_; }

    // =================================================================
    // WORLDINTERFACE IMPLEMENTATION - PRESSURE SYSTEM
    // =================================================================

    void setPressureSystem(PressureSystem system) override { pressure_system_ = system; }
    PressureSystem getPressureSystem() const override { return pressure_system_; }

    // =================================================================
    // WORLDINTERFACE IMPLEMENTATION - DUAL PRESSURE SYSTEM
    // =================================================================

    void setHydrostaticPressureEnabled(bool enabled) override
    {
        hydrostatic_pressure_enabled_ = enabled;
    }
    bool isHydrostaticPressureEnabled() const override { return hydrostatic_pressure_enabled_; }

    void setDynamicPressureEnabled(bool enabled) override { dynamic_pressure_enabled_ = enabled; }
    bool isDynamicPressureEnabled() const override { return dynamic_pressure_enabled_; }

    void setHydrostaticPressureStrength(double strength) override
    {
        hydrostatic_pressure_strength_ = strength;
    }
    double getHydrostaticPressureStrength() const override
    {
        return hydrostatic_pressure_strength_;
    }

    void setDynamicPressureStrength(double strength) override
    {
        dynamic_pressure_strength_ = strength;
    }
    double getDynamicPressureStrength() const override { return dynamic_pressure_strength_; }

    // Pressure calculator access
    WorldBPressureCalculator& getPressureCalculator() { return pressure_calculator_; }
    const WorldBPressureCalculator& getPressureCalculator() const { return pressure_calculator_; }

    // Collision calculator access
    WorldBCollisionCalculator& getCollisionCalculator() { return collision_calculator_; }
    const WorldBCollisionCalculator& getCollisionCalculator() const
    {
        return collision_calculator_;
    }

    // Pressure system getters for calculator
    double getPressureScale() const { return pressure_scale_; }
    Vector2d getGravityVector() const { return Vector2d(0.0, gravity_); }

    // =================================================================
    // WORLDINTERFACE IMPLEMENTATION - TIME REVERSAL (NO-OP)
    // =================================================================

    void enableTimeReversal(bool /* enabled */) override { /* no-op */ }
    bool isTimeReversalEnabled() const override { return false; }
    void saveWorldState() override { /* no-op */ }
    bool canGoBackward() const override { return false; }
    bool canGoForward() const override { return false; }
    void goBackward() override { /* no-op */ }
    void goForward() override { /* no-op */ }
    void clearHistory() override { /* no-op */ }
    size_t getHistorySize() const override { return 0; }

    // WORLDINTERFACE IMPLEMENTATION - WORLD SETUP (SIMPLIFIED)
    // WorldB-specific wall setup behavior (overrides base class)
    void setWallsEnabled(bool enabled) override;
    bool areWallsEnabled() const override; // WorldB defaults to true instead of false

    // WORLDINTERFACE IMPLEMENTATION - CURSOR FORCE
    void setCursorForceEnabled(bool enabled) override { cursor_force_enabled_ = enabled; }
    void updateCursorForce(int pixelX, int pixelY, bool isActive) override;
    void clearCursorForce() override { cursor_force_active_ = false; }

    // WORLDINTERFACE IMPLEMENTATION - COHESION PHYSICS CONTROL
    void setCohesionBindForceEnabled(bool enabled) override
    {
        cohesion_bind_force_enabled_ = enabled;
    }
    bool isCohesionBindForceEnabled() const override { return cohesion_bind_force_enabled_; }

    void setCohesionComForceEnabled(bool enabled) override
    {
        cohesion_com_force_enabled_ = enabled;
    }
    bool isCohesionComForceEnabled() const override { return cohesion_com_force_enabled_; }

    void setCohesionComForceStrength(double strength) override
    {
        cohesion_com_force_strength_ = strength;
    }
    double getCohesionComForceStrength() const override { return cohesion_com_force_strength_; }

    void setAdhesionStrength(double strength) override
    {
        adhesion_calculator_.setAdhesionStrength(strength);
    }
    double getAdhesionStrength() const override
    {
        return adhesion_calculator_.getAdhesionStrength();
    }

    void setAdhesionEnabled(bool enabled) override
    {
        adhesion_calculator_.setAdhesionEnabled(enabled);
    }
    bool isAdhesionEnabled() const override { return adhesion_calculator_.isAdhesionEnabled(); }

    void setCohesionBindForceStrength(double strength) override
    {
        cohesion_bind_force_strength_ = strength;
    }
    double getCohesionBindForceStrength() const override { return cohesion_bind_force_strength_; }

    void setCOMCohesionRange(uint32_t range) override { com_cohesion_range_ = range; }
    uint32_t getCOMCohesionRange() const override { return com_cohesion_range_; }

    // WORLDINTERFACE IMPLEMENTATION - AIR RESISTANCE CONTROL
    void setAirResistanceEnabled(bool enabled) override { air_resistance_enabled_ = enabled; }
    bool isAirResistanceEnabled() const override { return air_resistance_enabled_; }
    void setAirResistanceStrength(double strength) override { air_resistance_strength_ = strength; }
    double getAirResistanceStrength() const override { return air_resistance_strength_; }

    // COM cohesion mode control
    enum class COMCohesionMode {
        ORIGINAL,  // Current implementation
        CENTERING, // Force toward cell center (0,0)
        MASS_BASED // New mass-based with cutoff
    };

    void setCOMCohesionMode(COMCohesionMode mode) { com_cohesion_mode_ = mode; }
    COMCohesionMode getCOMCohesionMode() const { return com_cohesion_mode_; }

    // WORLDINTERFACE IMPLEMENTATION - GRID MANAGEMENT
    void resizeGrid(uint32_t newWidth, uint32_t newHeight) override;
    void markAllCellsDirty() override;

    // WORLDINTERFACE IMPLEMENTATION - PERFORMANCE AND DEBUGGING
    void dumpTimerStats() const override { timers_.dumpTimerStats(); }
    void markUserInput() override { /* no-op for now */ }
    std::string settingsToString() const override;

    // WORLDINTERFACE IMPLEMENTATION - WORLD TYPE MANAGEMENT
    WorldType getWorldType() const override;
    void preserveState(::WorldState& state) const override;
    void restoreState(const ::WorldState& state) override;

    // WORLDINTERFACE IMPLEMENTATION - UI INTEGRATION
    void setUI(std::unique_ptr<SimulatorUI> ui) override;
    void setUIReference(SimulatorUI* ui) override;
    SimulatorUI* getUI() const override { return ui_ref_ ? ui_ref_ : ui_.get(); }

    // World type identification
    const char* getWorldTypeName() const override { return "WorldB (RulesB)"; }

    // =================================================================
    // WORLDB-SPECIFIC METHODS
    // =================================================================

    // Direct cell access
    CellB& at(uint32_t x, uint32_t y);
    const CellB& at(uint32_t x, uint32_t y) const;
    CellB& at(const Vector2i& pos);
    const CellB& at(const Vector2i& pos) const;

    // Add material at specific cell coordinates
    void addMaterialAtCell(uint32_t x, uint32_t y, MaterialType type, double amount = 1.0) override;

    // Physics constants from GridMechanics.md (all per-timestep values)
    static constexpr double MAX_VELOCITY_PER_TIMESTEP = 20.0; // cells/timestep
    static constexpr double VELOCITY_DAMPING_THRESHOLD_PER_TIMESTEP =
        10.0; // velocity threshold for damping (cells/timestep)
    static constexpr double VELOCITY_DAMPING_FACTOR_PER_TIMESTEP =
        0.10;                                             // 10% slowdown per timestep
    static constexpr double MIN_MATTER_THRESHOLD = 0.001; // minimum matter to process

    // Distance-based cohesion decay constants
    static constexpr double SUPPORT_DECAY_RATE = 0.3; // Decay rate per distance unit
    static constexpr double MIN_SUPPORT_FACTOR =
        0.05; // Minimum cohesion factor (never goes to zero)
    static constexpr double MAX_SUPPORT_DISTANCE =
        10; // Maximum search distance for support (legacy)

    // Directional support constants for realistic physics
    static constexpr double MAX_VERTICAL_SUPPORT_DISTANCE =
        5; // Check 5 cells down for vertical support
    static constexpr double RIGID_DENSITY_THRESHOLD =
        5.0; // Materials above this density provide rigid support

    // Mass-based COM cohesion constants
    static constexpr double COM_COHESION_INNER_THRESHOLD =
        0.5; // COM must be > 0.5 from center to activate
    static constexpr double COM_COHESION_MIN_DISTANCE = 0.1; // Prevent division by near-zero
    static constexpr double COM_COHESION_MAX_FORCE = 5.0;    // Cap maximum force magnitude
    static constexpr double STRONG_ADHESION_THRESHOLD =
        0.5; // Minimum adhesion needed for horizontal support

    // =================================================================
    // TESTING METHODS
    // =================================================================

    // Clear pending moves for testing
    void clearPendingMoves() { pending_moves_.clear(); }

    // Expose cells array for static method testing
    const CellB* getCellsData() const { return cells_.data(); }

    // =================================================================
    // FORCE CALCULATION METHODS
    // =================================================================

    // Calculate adhesion force from different-material neighbors

    // Support calculation methods moved to WorldBSupportCalculator
    WorldBSupportCalculator& getSupportCalculator() { return support_calculator_; }
    const WorldBSupportCalculator& getSupportCalculator() const { return support_calculator_; }

    // Adhesion calculation methods moved to WorldBAdhesionCalculator
    WorldBAdhesionCalculator& getAdhesionCalculator() { return adhesion_calculator_; }
    const WorldBAdhesionCalculator& getAdhesionCalculator() const { return adhesion_calculator_; }

    // Material transfer computation - computes moves without processing them
    std::vector<MaterialMove> computeMaterialMoves(double deltaTime);

protected:
    // WorldInterface hook implementations
    void onPostResize() override;

private:
    // =================================================================
    // INTERNAL PHYSICS METHODS
    // =================================================================

    // Physics simulation steps
    void applyGravity(double deltaTime);
    void applyAirResistance(double deltaTime);
    void applyCohesionForces(double deltaTime);
    void resolveForces(double deltaTime); // Apply accumulated forces based on resistance
    void updateTransfers(double deltaTime);
    void processVelocityLimiting(double deltaTime);

    // Material transfer system
    void processMaterialMoves();

    // Boundary wall management
    void setupBoundaryWalls();

    // Coordinate conversion helpers
    void pixelToCell(int pixelX, int pixelY, int& cellX, int& cellY) const;
    Vector2i pixelToCell(int pixelX, int pixelY) const;
    bool isValidCell(int x, int y) const;
    bool isValidCell(const Vector2i& pos) const;
    size_t coordToIndex(uint32_t x, uint32_t y) const;
    size_t coordToIndex(const Vector2i& pos) const;

    // World position calculation for COM cohesion forces
    Vector2d getCellWorldPosition(uint32_t x, uint32_t y, const Vector2d& com_offset) const;

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

    // Dual pressure system controls
    bool hydrostatic_pressure_enabled_;
    bool dynamic_pressure_enabled_;
    double hydrostatic_pressure_strength_;
    double dynamic_pressure_strength_;

    // World setup controls
    bool add_particles_enabled_;

    // Cursor force state
    bool cursor_force_enabled_;
    bool cursor_force_active_;
    int cursor_force_x_;
    int cursor_force_y_;

    // Cohesion physics control
    bool cohesion_bind_force_enabled_;    // Enable/disable cohesion bind force (resistance)
    bool cohesion_com_force_enabled_;     // Enable/disable cohesion COM force (attraction)
    COMCohesionMode com_cohesion_mode_;   // COM cohesion calculation mode
    double cohesion_com_force_strength_;  // Scaling factor for COM cohesion force magnitude
    double cohesion_bind_force_strength_; // Scaling factor for cohesion bind resistance
    uint32_t com_cohesion_range_;         // Range for COM cohesion neighbors (default 2)

    // Air resistance control
    bool air_resistance_enabled_;    // Enable/disable air resistance forces
    double air_resistance_strength_; // Strength multiplier for air resistance

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

    // Dynamic pressure system

    // Performance timing
    mutable Timers timers_;

    // Support calculation
    mutable WorldBSupportCalculator support_calculator_;

    // Pressure calculation
    WorldBPressureCalculator pressure_calculator_;

    // Collision calculation
    WorldBCollisionCalculator collision_calculator_;

    // Adhesion calculation
    mutable WorldBAdhesionCalculator adhesion_calculator_;

    // UI interface
    std::unique_ptr<SimulatorUI> ui_; // Owned UI (legacy architecture)
    SimulatorUI* ui_ref_;             // Non-owning reference (SimulationManager architecture)
};
