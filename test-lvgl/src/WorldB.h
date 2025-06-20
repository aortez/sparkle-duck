#pragma once

#include "CellB.h"
#include "MaterialType.h"
#include "Timers.h"
#include "Vector2i.h"
#include "WorldBCohesionCalculator.h"
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
    // Force calculation structures for adhesion physics.
    struct AdhesionForce {
        Vector2d force_direction;     // Direction of adhesive pull/resistance
        double force_magnitude;       // Strength of adhesive force
        MaterialType target_material; // Strongest interacting material
        uint32_t contact_points;      // Number of contact interfaces
    };

    // Enhanced material transfer system with collision physics
    enum class CollisionType {
        TRANSFER_ONLY,       // Material moves between cells (current behavior)
        ELASTIC_REFLECTION,  // Bouncing with energy conservation
        INELASTIC_COLLISION, // Bouncing with energy loss
        FRAGMENTATION,       // Break apart into smaller pieces
        ABSORPTION           // One material absorbs the other
    };

    struct MaterialMove {
        int fromX, fromY;
        int toX, toY;
        double amount;
        MaterialType material;
        Vector2d momentum;
        Vector2d boundary_normal; // Direction of boundary crossing for physics

        // NEW: Collision-specific data
        CollisionType collision_type = CollisionType::TRANSFER_ONLY;
        double collision_energy = 0.0;        // Calculated impact energy
        double restitution_coefficient = 0.0; // Material-specific bounce factor
        double material_mass = 0.0;           // Mass of moving material
        double target_mass = 0.0;             // Mass of target material (if any)

        // NEW: COM cohesion force data
        double com_cohesion_magnitude = 0.0;         // Strength of COM cohesion force
        Vector2d com_cohesion_direction{ 0.0, 0.0 }; // Direction of COM cohesion force
    };

    // Blocked transfer data for dynamic pressure accumulation
    struct BlockedTransfer {
        int fromX, fromY;          // Source cell coordinates
        double blocked_amount;     // Amount that failed to transfer
        MaterialType material;     // Material type that was blocked
        Vector2d blocked_velocity; // Velocity vector of blocked material
        Vector2d boundary_normal;  // Direction of attempted transfer
        double blocked_energy;     // Kinetic energy that was blocked

        BlockedTransfer(
            int x,
            int y,
            double amount,
            MaterialType mat,
            const Vector2d& velocity,
            const Vector2d& normal)
            : fromX(x),
              fromY(y),
              blocked_amount(amount),
              material(mat),
              blocked_velocity(velocity),
              boundary_normal(normal),
              blocked_energy(velocity.magnitude() * amount)
        {}
    };

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

    // Pressure calculation methods (public for testing)
    void calculateHydrostaticPressure();
    Vector2d calculatePressureForce(const CellB& cell) const;
    double getHydrostaticWeight(MaterialType material) const;
    double getDynamicWeight(MaterialType material) const;

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
    void setCohesionEnabled(bool enabled) override { cohesion_enabled_ = enabled; }
    bool isCohesionEnabled() const override { return cohesion_enabled_; }

    void setCohesionForceEnabled(bool enabled) override { cohesion_force_enabled_ = enabled; }
    bool isCohesionForceEnabled() const override { return cohesion_force_enabled_; }

    void setCohesionForceStrength(double strength) override { cohesion_force_strength_ = strength; }
    double getCohesionForceStrength() const override { return cohesion_force_strength_; }

    void setAdhesionStrength(double strength) override { adhesion_strength_ = strength; }
    double getAdhesionStrength() const override { return adhesion_strength_; }

    void setAdhesionEnabled(bool enabled) override { adhesion_enabled_ = enabled; }
    bool isAdhesionEnabled() const override { return adhesion_enabled_; }

    void setCohesionBindStrength(double strength) override { cohesion_bind_strength_ = strength; }
    double getCohesionBindStrength() const override { return cohesion_bind_strength_; }

    void setCOMCohesionRange(uint32_t range) override { com_cohesion_range_ = range; }
    uint32_t getCOMCohesionRange() const override { return com_cohesion_range_; }

    // WORLDINTERFACE IMPLEMENTATION - GRID MANAGEMENT
    void resizeGrid(uint32_t newWidth, uint32_t newHeight) override;

    // WORLDINTERFACE IMPLEMENTATION - PERFORMANCE AND DEBUGGING
    void dumpTimerStats() const override { timers_.dumpTimerStats(); }
    void markUserInput() override { /* no-op for now */ }

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
        0.1; // Minimum cohesion factor (never goes to zero)
    static constexpr double MAX_SUPPORT_DISTANCE =
        10; // Maximum search distance for support (legacy)

    // Directional support constants for realistic physics
    static constexpr double MAX_VERTICAL_SUPPORT_DISTANCE =
        5; // Check 5 cells down for vertical support
    static constexpr double RIGID_DENSITY_THRESHOLD =
        5.0; // Materials above this density provide rigid support
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
    AdhesionForce calculateAdhesionForce(uint32_t x, uint32_t y);

    // Support calculation methods moved to WorldBSupportCalculator
    WorldBSupportCalculator& getSupportCalculator() { return support_calculator_; }
    const WorldBSupportCalculator& getSupportCalculator() const { return support_calculator_; }
    
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
    void applyCohesionForces(double deltaTime);
    void updateTransfers(double deltaTime);
    void applyPressure(double deltaTime);
    void processVelocityLimiting(double deltaTime);

    // Material transfer system
    void processMaterialMoves();

    // Enhanced collision detection and move creation
    std::vector<Vector2i> getAllBoundaryCrossings(const Vector2d& newCOM);
    MaterialMove createCollisionAwareMove(
        const CellB& fromCell,
        const CellB& toCell,
        const Vector2i& fromPos,
        const Vector2i& toPos,
        const Vector2i& direction,
        double deltaTime = 0.0,
        const WorldBCohesionCalculator::COMCohesionForce& com_cohesion = {
            { 0.0, 0.0 }, 0.0, { 0.0, 0.0 }, 0 });
    CollisionType determineCollisionType(
        MaterialType from, MaterialType to, double collision_energy);

    // Collision physics calculations
    double calculateMaterialMass(const CellB& cell);
    double calculateCollisionEnergy(
        const MaterialMove& move, const CellB& fromCell, const CellB& toCell);

    // Collision handlers
    void handleTransferMove(CellB& fromCell, CellB& toCell, const MaterialMove& move);
    void handleElasticCollision(CellB& fromCell, CellB& toCell, const MaterialMove& move);
    void handleInelasticCollision(CellB& fromCell, CellB& toCell, const MaterialMove& move);
    void handleFragmentation(CellB& fromCell, CellB& toCell, const MaterialMove& move);
    void handleAbsorption(CellB& fromCell, CellB& toCell, const MaterialMove& move);

    // Floating particle collision detection
    bool checkFloatingParticleCollision(int cellX, int cellY);
    void handleFloatingParticleCollision(int cellX, int cellY);

    // Dynamic pressure system
    void queueBlockedTransfer(
        int fromX,
        int fromY,
        double blocked_amount,
        MaterialType material,
        const Vector2d& velocity,
        const Vector2d& boundary_normal);
    void processBlockedTransfers();
    void applyDynamicPressureForces(double deltaTime);

    // Pressure calculation (simplified hydrostatic) - moved to public for testing

    // Boundary wall management
    void setupBoundaryWalls();

    // Elastic boundary reflection system
    void applyBoundaryReflection(CellB& cell, const Vector2i& direction);
    void applyCellBoundaryReflection(CellB& cell, const Vector2i& direction, MaterialType material);

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

    // World setup controls
    bool add_particles_enabled_;

    // Cursor force state
    bool cursor_force_enabled_;
    bool cursor_force_active_;
    int cursor_force_x_;
    int cursor_force_y_;

    // Cohesion physics control
    bool cohesion_enabled_;
    bool cohesion_force_enabled_;
    bool adhesion_enabled_;          // Enable/disable adhesion physics
    double cohesion_force_strength_; // Scaling factor for COM cohesion force magnitude
    double adhesion_strength_;       // Scaling factor for adhesion force magnitude
    double cohesion_bind_strength_;  // Scaling factor for cohesion resistance
    uint32_t com_cohesion_range_;    // Range for COM cohesion neighbors (default 2)

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
    std::vector<BlockedTransfer> blocked_transfers_;

    // Performance timing
    mutable Timers timers_;

    // Support calculation
    mutable WorldBSupportCalculator support_calculator_;

    // UI interface
    std::unique_ptr<SimulatorUI> ui_; // Owned UI (legacy architecture)
    SimulatorUI* ui_ref_;             // Non-owning reference (SimulationManager architecture)
};
