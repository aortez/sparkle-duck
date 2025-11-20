#pragma once

#include "Cell.h"
#include "MaterialMove.h"
#include "MaterialType.h"
#include "PhysicsSettings.h"
#include "Timers.h"
#include "Vector2i.h"
#include "WorldAdhesionCalculator.h"
#include "WorldCohesionCalculator.h"
#include "WorldCollisionCalculator.h"
#include "WorldData.h"
#include "WorldFrictionCalculator.h"
#include "WorldPressureCalculator.h"
#include "WorldSupportCalculator.h"
#include "WorldViscosityCalculator.h"
#include "organisms/TreeTypes.h"

#include <cstdint>
#include <memory>
#include <random>
#include <vector>

/**
 * \file
 * World implements the pure-material physics system based on GridMechanics.md.
 * Unlike World (mixed dirt/water), World uses Cell with pure materials and
 * fill ratios, providing a simpler but different physics model.
 */

namespace DirtSim {

// Forward declarations.
class TreeManager;

class World {
public:
    // Motion states for viscosity calculations.
    enum class MotionState {
        STATIC,   // Supported by surface, minimal velocity.
        FALLING,  // No support, downward velocity.
        SLIDING,  // Moving along a surface with support.
        TURBULENT // High velocity differences with neighbors.
    };

    World();
    World(uint32_t width, uint32_t height);
    ~World();

    // Copy and move - trivially copyable now that calculators are stateless!
    World(const World& other) = default;
    World& operator=(const World& other) = default;
    World(World&&) = default;
    World& operator=(World&&) = default;

    // =================================================================
    // WORLDINTERFACE IMPLEMENTATION - CORE SIMULATION
    // =================================================================

    void advanceTime(double deltaTimeSeconds);
    void reset();
    void setup();
    void applyPhysicsSettings(const PhysicsSettings& settings);

    // =================================================================
    // WORLDINTERFACE IMPLEMENTATION - GRID ACCESS
    // =================================================================

    // Direct cell access.
    Cell& at(uint32_t x, uint32_t y);
    const Cell& at(uint32_t x, uint32_t y) const;
    Cell& at(const Vector2i& pos);
    const Cell& at(const Vector2i& pos) const;

    // =================================================================
    // WORLDINTERFACE IMPLEMENTATION - SIMULATION CONTROL
    // =================================================================

    // NOTE: Use physicsSettings.timescale for physics, data.removed_mass,
    // data.add_particles_enabled directly.
    double getTotalMass() const;

    // =================================================================
    // WORLDINTERFACE IMPLEMENTATION - MATERIAL ADDITION
    // =================================================================

    // Material selection state management (for UI/API coordination).
    void setSelectedMaterial(MaterialType type) { selected_material_ = type; }
    MaterialType getSelectedMaterial() const { return selected_material_; }

    // =================================================================
    // WORLDINTERFACE IMPLEMENTATION - PHYSICS PARAMETERS
    // =================================================================

    Vector2d getGravityVector() const { return Vector2d{ 0.0, physicsSettings.gravity }; }
    void setDirtFragmentationFactor(double /* factor */) { /* no-op for World */ }

    // =================================================================
    // WORLDINTERFACE IMPLEMENTATION - PRESSURE SYSTEM
    // =================================================================

    // =================================================================
    // WORLDINTERFACE IMPLEMENTATION - DUAL PRESSURE SYSTEM
    // =================================================================
    // Use physicsSettings.pressure_*_enabled/strength directly instead of setters.

    bool isHydrostaticPressureEnabled() const
    {
        return physicsSettings.pressure_hydrostatic_strength > 0.0;
    }

    bool isDynamicPressureEnabled() const
    {
        return physicsSettings.pressure_dynamic_strength > 0.0;
    }

    bool isPressureDiffusionEnabled() const
    {
        return physicsSettings.pressure_diffusion_strength > 0.0;
    }

    void setHydrostaticPressureStrength(double strength);
    double getHydrostaticPressureStrength() const;

    void setDynamicPressureStrength(double strength);
    double getDynamicPressureStrength() const;

    // Pressure calculator access.
    WorldPressureCalculator& getPressureCalculator() { return pressure_calculator_; }
    const WorldPressureCalculator& getPressureCalculator() const { return pressure_calculator_; }

    // Collision calculator access.
    WorldCollisionCalculator& getCollisionCalculator() { return collision_calculator_; }
    const WorldCollisionCalculator& getCollisionCalculator() const { return collision_calculator_; }

    // =================================================================
    // WORLDINTERFACE IMPLEMENTATION - TIME REVERSAL (NO-OP)
    // =================================================================

    void enableTimeReversal(bool /* enabled */) { /* no-op */ }
    bool isTimeReversalEnabled() const { return false; }
    void saveWorldState() { /* no-op */ }
    bool canGoBackward() const { return false; }
    bool canGoForward() const { return false; }
    void goBackward() { /* no-op */ }
    void goForward() { /* no-op */ }
    void clearHistory() { /* no-op */ }
    size_t getHistorySize() const { return 0; }

    // WORLDINTERFACE IMPLEMENTATION - WORLD SETUP (SIMPLIFIED)
    // World-specific wall setup behavior (s base class)
    void setWallsEnabled(bool enabled);
    bool areWallsEnabled() const; // World defaults to true instead of false

    // WORLDINTERFACE IMPLEMENTATION - COHESION PHYSICS CONTROL
    void setCohesionBindForceEnabled(bool enabled) { cohesion_bind_force_enabled_ = enabled; }
    bool isCohesionBindForceEnabled() const { return cohesion_bind_force_enabled_; }

    void setCohesionComForceEnabled(bool enabled)
    {
        // Use PhysicsSettings as single source of truth.
        physicsSettings.cohesion_enabled = enabled;
        physicsSettings.cohesion_strength = enabled ? 150.0 : 0.0;
    }
    bool isCohesionComForceEnabled() const { return physicsSettings.cohesion_strength > 0.0; }

    void setCohesionComForceStrength(double strength)
    {
        physicsSettings.cohesion_strength = strength;
    }
    double getCohesionComForceStrength() const { return physicsSettings.cohesion_strength; }

    void setAdhesionStrength(double strength) { physicsSettings.adhesion_strength = strength; }
    double getAdhesionStrength() const { return physicsSettings.adhesion_strength; }

    void setAdhesionEnabled(bool enabled)
    {
        physicsSettings.adhesion_enabled = enabled;
        physicsSettings.adhesion_strength = enabled ? 5.0 : 0.0;
    }
    bool isAdhesionEnabled() const { return physicsSettings.adhesion_strength > 0.0; }

    void setCohesionBindForceStrength(double strength) { cohesion_bind_force_strength_ = strength; }
    double getCohesionBindForceStrength() const { return cohesion_bind_force_strength_; }

    // Viscosity control.
    void setViscosityStrength(double strength) { physicsSettings.viscosity_strength = strength; }
    double getViscosityStrength() const { return physicsSettings.viscosity_strength; }

    // Friction control (velocity-dependent viscosity).
    void setFrictionStrength(double strength) { physicsSettings.friction_strength = strength; }
    double getFrictionStrength() const { return physicsSettings.friction_strength; }

    // Friction calculator access.
    WorldFrictionCalculator& getFrictionCalculator() { return friction_calculator_; }
    const WorldFrictionCalculator& getFrictionCalculator() const { return friction_calculator_; }

    void setCOMCohesionRange(uint32_t range) { com_cohesion_range_ = range; }
    uint32_t getCOMCohesionRange() const { return com_cohesion_range_; }

    // Motion state multiplier calculation (for viscosity and other systems).
    double getMotionStateMultiplier(MotionState state, double sensitivity) const;

    // WORLDINTERFACE IMPLEMENTATION - AIR RESISTANCE CONTROL
    void setAirResistanceEnabled(bool enabled) { air_resistance_enabled_ = enabled; }
    bool isAirResistanceEnabled() const { return air_resistance_enabled_; }
    void setAirResistanceStrength(double strength) { air_resistance_strength_ = strength; }
    double getAirResistanceStrength() const { return air_resistance_strength_; }

    // COM cohesion mode removed - always uses ORIGINAL implementation

    // WORLDINTERFACE IMPLEMENTATION - GRID MANAGEMENT
    void resizeGrid(uint32_t newWidth, uint32_t newHeight);

    // WORLDINTERFACE IMPLEMENTATION - PERFORMANCE AND DEBUGGING
    void dumpTimerStats() const { timers_.dumpTimerStats(); }
    void markUserInput() { /* no-op for now */ }
    std::string settingsToString() const;

    // World setup management (DEPRECATED - scenarios now handle setup directly).

    // =================================================================
    // WORLD-SPECIFIC METHODS
    // =================================================================

    // Add material at specific cell coordinates.
    void addMaterialAtCell(uint32_t x, uint32_t y, MaterialType type, double amount = 1.0);

    /**
     * Record an organism material transfer for efficient TreeManager tracking.
     * Called during physics transfers to maintain organism ownership consistency.
     */
    void recordOrganismTransfer(
        int fromX, int fromY, int toX, int toY, TreeId organism_id, double amount);

    static constexpr double MIN_MATTER_THRESHOLD = 0.001; // minimum matter to process.

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
    // FORCE CALCULATION METHODS
    // =================================================================

    // Material transfer computation - computes moves without processing them.
    std::vector<MaterialMove> computeMaterialMoves(double deltaTime);

    // =================================================================
    // JSON SERIALIZATION
    // =================================================================

    // Serialize complete world state to JSON (lossless).
    nlohmann::json toJSON() const;

    // Deserialize world state from JSON.
    void fromJSON(const nlohmann::json& doc);

    // =================================================================
    // UTILITY METHODS
    // =================================================================

    std::string toAsciiDiagram() const;

    // Stub methods for unimplemented features (TODO: remove event handlers that call these).
    void setRainRate(double) {}
    double getRainRate() const { return 0.0; }
    void spawnMaterialBall(MaterialType material, uint32_t centerX, uint32_t centerY);
    void setWaterColumnEnabled(bool) {}
    bool isWaterColumnEnabled() const { return false; }
    void setLeftThrowEnabled(bool) {}
    bool isLeftThrowEnabled() const { return false; }
    void setRightThrowEnabled(bool) {}
    bool isRightThrowEnabled() const { return false; }
    void setLowerRightQuadrantEnabled(bool) {}
    bool isLowerRightQuadrantEnabled() const { return false; }

    // World state data - public source of truth for all serializable state.
    WorldData data;

    // Physics settings - public source of truth for physics parameters.
    PhysicsSettings physicsSettings;

    // WorldInterface hook implementations (rarely overridden - can be public).
    void onPostResize();
    void onPreResize(uint32_t newWidth, uint32_t newHeight);
    bool shouldResize(uint32_t newWidth, uint32_t newHeight) const;

    // =================================================================
    // CONFIGURATION (public - direct access preferred)
    // =================================================================

    // Physics parameters (TODO: migrate to WorldData).
    // NOTE: Most physics parameters now use physicsSettings as single source of truth.
    bool cohesion_bind_force_enabled_;
    double cohesion_bind_force_strength_;
    uint32_t com_cohesion_range_;
    bool air_resistance_enabled_;
    double air_resistance_strength_;
    MaterialType selected_material_;

    // =================================================================
    // CALCULATORS (public for direct access)
    // =================================================================

    WorldSupportCalculator support_calculator_;
    WorldPressureCalculator pressure_calculator_;
    WorldCollisionCalculator collision_calculator_;
    WorldAdhesionCalculator adhesion_calculator_;
    WorldFrictionCalculator friction_calculator_;
    WorldViscosityCalculator viscosity_calculator_;

    // Material transfer queue (internal simulation state).
    std::vector<MaterialMove> pending_moves_;

    // Organism transfer tracking (for efficient TreeManager updates).
    std::vector<OrganismTransfer> organism_transfers_;

    // Performance timing.
    mutable Timers timers_;

    // Tree organism manager.
    std::unique_ptr<class TreeManager> tree_manager_;

    // Accessor for tree manager.
    class TreeManager& getTreeManager() { return *tree_manager_; }
    const class TreeManager& getTreeManager() const { return *tree_manager_; }

    // Per-world random number generator for deterministic testing.
    std::unique_ptr<std::mt19937> rng_;

    // Set RNG seed (for deterministic testing).
    void setRandomSeed(uint32_t seed);

private:
    // =================================================================
    // INTERNAL PHYSICS METHODS (implementation details)
    // =================================================================

    void applyGravity();
    void applyAirResistance();
    void applyCohesionForces();
    void applyPressureForces();
    void resolveForces(double deltaTime, const GridOfCells* grid = nullptr);
    void updateTransfers(double deltaTime);
    void processVelocityLimiting(double deltaTime);
    void processMaterialMoves();
    void setupBoundaryWalls();

    // Coordinate conversion helpers (can be public if needed).
    void pixelToCell(int pixelX, int pixelY, int& cellX, int& cellY) const;
    Vector2i pixelToCell(int pixelX, int pixelY) const;
    bool isValidCell(int x, int y) const;
    bool isValidCell(const Vector2i& pos) const;
    size_t coordToIndex(uint32_t x, uint32_t y) const;
    size_t coordToIndex(const Vector2i& pos) const;
};

/**
 * ADL (Argument-Dependent Lookup) functions for nlohmann::json automatic conversion.
 */
void to_json(nlohmann::json& j, World::MotionState state);
void from_json(const nlohmann::json& j, World::MotionState& state);

} // namespace DirtSim
