#pragma once

#include "Cell.h"
#include "MaterialMove.h"
#include "MaterialType.h"
#include "Timers.h"
#include "Vector2i.h"
#include "WorldAdhesionCalculator.h"
#include "WorldCohesionCalculator.h"
#include "WorldCollisionCalculator.h"
#include "PhysicsSettings.h"
#include "WorldData.h"
#include "WorldEventGenerator.h"
#include "WorldFrictionCalculator.h"
#include "WorldPressureCalculator.h"
#include "WorldSupportCalculator.h"

#include <cstdint>
#include <memory>
#include <vector>

/**
 * \file
 * World implements the pure-material physics system based on GridMechanics.md.
 * Unlike World (mixed dirt/water), World uses Cell with pure materials and
 * fill ratios, providing a simpler but different physics model.
 */

namespace DirtSim {

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

    // NOTE: Use physicsSettings.timescale for physics, data.removed_mass, data.add_particles_enabled directly.
    double getTotalMass() const;

    // =================================================================
    // WORLDINTERFACE IMPLEMENTATION - MATERIAL ADDITION
    // =================================================================

    // Universal material addition (direct support for all 8 material types).
    void addMaterialAtPixel(int pixelX, int pixelY, MaterialType type, double amount = 1.0);

    // Material selection state management (for UI/API coordination).
    void setSelectedMaterial(MaterialType type) { selected_material_ = type; }
    MaterialType getSelectedMaterial() const { return selected_material_; }

    // =================================================================
    // WORLDINTERFACE IMPLEMENTATION - PHYSICS PARAMETERS
    // =================================================================

    Vector2d getGravityVector() const { return Vector2d{ 0.0, physicsSettings.gravity }; }
    void setDirtFragmentationFactor(double /* factor */) { /* no-op for World */ }

    // =================================================================
    // WORLDINTERFACE IMPLEMENTATION - WATER PHYSICS (SIMPLIFIED)
    // =================================================================

    void setWaterPressureThreshold(double threshold) { water_pressure_threshold_ = threshold; }
    double getWaterPressureThreshold() const { return water_pressure_threshold_; }

    // =================================================================
    // WORLDINTERFACE IMPLEMENTATION - PRESSURE SYSTEM
    // =================================================================

    // =================================================================
    // WORLDINTERFACE IMPLEMENTATION - DUAL PRESSURE SYSTEM
    // =================================================================

    void setHydrostaticPressureEnabled(bool enabled);
    bool isHydrostaticPressureEnabled() const { return physicsSettings.pressure_hydrostatic_strength > 0.0; }

    void setDynamicPressureEnabled(bool enabled);
    bool isDynamicPressureEnabled() const { return physicsSettings.pressure_dynamic_strength > 0.0; }

    void setPressureDiffusionEnabled(bool enabled) { physicsSettings.pressure_diffusion_strength = enabled ? 1.0 : 0.0; }
    bool isPressureDiffusionEnabled() const { return physicsSettings.pressure_diffusion_strength > 0.0; }

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
        // Backward compatibility: set strength to 0 (disabled) or default (enabled).
        cohesion_com_force_strength_ = enabled ? 150.0 : 0.0;
    }
    bool isCohesionComForceEnabled() const { return cohesion_com_force_strength_ > 0.0; }

    void setCohesionComForceStrength(double strength) { cohesion_com_force_strength_ = strength; }
    double getCohesionComForceStrength() const { return cohesion_com_force_strength_; }

    void setAdhesionStrength(double strength)
    {
        adhesion_calculator_.setAdhesionStrength(strength);
    }
    double getAdhesionStrength() const { return adhesion_calculator_.getAdhesionStrength(); }

    void setAdhesionEnabled(bool enabled) { adhesion_calculator_.setAdhesionEnabled(enabled); }
    bool isAdhesionEnabled() const { return adhesion_calculator_.isAdhesionEnabled(); }

    void setCohesionBindForceStrength(double strength) { cohesion_bind_force_strength_ = strength; }
    double getCohesionBindForceStrength() const { return cohesion_bind_force_strength_; }

    // Viscosity control.
    void setViscosityStrength(double strength) { viscosity_strength_ = strength; }
    double getViscosityStrength() const { return viscosity_strength_; }

    // Friction control (velocity-dependent viscosity).
    void setFrictionStrength(double strength) { friction_strength_ = strength; }
    double getFrictionStrength() const { return friction_strength_; }

    // Friction calculator access.
    WorldFrictionCalculator& getFrictionCalculator() { return friction_calculator_; }
    const WorldFrictionCalculator& getFrictionCalculator() const { return friction_calculator_; }

    void setCOMCohesionRange(uint32_t range) { com_cohesion_range_ = range; }
    uint32_t getCOMCohesionRange() const { return com_cohesion_range_; }

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

    // World setup management
    void setWorldEventGenerator(std::shared_ptr<WorldEventGenerator> setup);
    WorldEventGenerator* getWorldEventGenerator() const;

    // =================================================================
    // WORLD-SPECIFIC METHODS
    // =================================================================

    // Add material at specific cell coordinates.
    void addMaterialAtCell(uint32_t x, uint32_t y, MaterialType type, double amount = 1.0);

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
    void spawnMaterialBall(
        MaterialType material, uint32_t centerX, uint32_t centerY, uint32_t radius);
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
    double water_pressure_threshold_;
    bool cohesion_bind_force_enabled_;
    double cohesion_com_force_strength_;
    double cohesion_bind_force_strength_;
    uint32_t com_cohesion_range_;
    double viscosity_strength_;
    double friction_strength_;
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

    // Material transfer queue (internal simulation state).
    std::vector<MaterialMove> pending_moves_;

    // Performance timing.
    mutable Timers timers_;

    // World event generator for dynamic particles.
    std::shared_ptr<WorldEventGenerator> worldEventGenerator_;

private:
    // =================================================================
    // INTERNAL PHYSICS METHODS (implementation details)
    // =================================================================

    void applyGravity();
    void applyAirResistance();
    void applyCohesionForces();
    void applyPressureForces();
    void resolveForces(double deltaTime);
    double getMotionStateMultiplier(MotionState state, double sensitivity) const;
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
