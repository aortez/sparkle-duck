#pragma once

#include "MaterialType.h"
#include "Pimpl.h"
#include "Vector2i.h"

#include <cstdint>
#include <memory>
#include <random>
#include <vector>

class Timers;

namespace DirtSim {
class Cell;
// Vector2d is now a template alias, include instead of forward declare.
#include "Vector2d.h"
struct MaterialMove;
struct WorldData;
struct PhysicsSettings;
class WorldPressureCalculator;
class WorldCollisionCalculator;
class WorldFrictionCalculator;
class WorldSupportCalculator;
class WorldAdhesionCalculator;
class WorldViscosityCalculator;
class GridOfCells;
} // namespace DirtSim

namespace DirtSim {

using TreeId = uint32_t;

class TreeManager;
struct OrganismTransfer;

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

    World(const World& other) = default;
    World& operator=(const World& other) = default;
    World(World&&) = default;
    World& operator=(World&&) = default;

    // =================================================================
    // CORE SIMULATION
    // =================================================================

    void advanceTime(double deltaTimeSeconds);
    void reset();
    void setup();
    void applyPhysicsSettings(const PhysicsSettings& settings);

    // =================================================================
    // SIMULATION CONTROL
    // =================================================================

    // NOTE: Use physicsSettings.timescale for physics, data.removed_mass,
    // data.add_particles_enabled directly.
    double getTotalMass() const;

    // =================================================================
    // MATERIAL ADDITION
    // =================================================================

    // Material selection state management (for UI/API coordination).
    void setSelectedMaterial(MaterialType type);
    MaterialType getSelectedMaterial() const;

    // =================================================================
    // PHYSICS PARAMETERS
    // =================================================================

    void setDirtFragmentationFactor(double factor);

    // =================================================================
    // PRESSURE SYSTEM
    // =================================================================

    // Use getPhysicsSettings() to access pressure settings directly.

    // Calculator access methods.
    WorldPressureCalculator& getPressureCalculator();
    const WorldPressureCalculator& getPressureCalculator() const;

    WorldCollisionCalculator& getCollisionCalculator();
    const WorldCollisionCalculator& getCollisionCalculator() const;

    // WorldSupportCalculator removed - now constructed locally with GridOfCells reference.

    WorldAdhesionCalculator& getAdhesionCalculator();
    const WorldAdhesionCalculator& getAdhesionCalculator() const;

    WorldViscosityCalculator& getViscosityCalculator();
    const WorldViscosityCalculator& getViscosityCalculator() const;

    // =================================================================
    // TIME REVERSAL (NO-OP)
    // =================================================================

    void enableTimeReversal(bool enabled);
    bool isTimeReversalEnabled() const;
    void saveWorldState();
    bool canGoBackward() const;
    bool canGoForward() const;
    void goBackward();
    void goForward();
    void clearHistory();
    size_t getHistorySize() const;

    // World-specific wall setup behavior
    void setWallsEnabled(bool enabled);
    bool areWallsEnabled() const; // World defaults to true instead of false

    // COHESION PHYSICS CONTROL
    void setCohesionBindForceEnabled(bool enabled);
    bool isCohesionBindForceEnabled() const;

    void setCohesionComForceEnabled(bool enabled);
    bool isCohesionComForceEnabled() const;

    void setCohesionComForceStrength(double strength);
    double getCohesionComForceStrength() const;

    void setAdhesionStrength(double strength);
    double getAdhesionStrength() const;

    void setAdhesionEnabled(bool enabled);
    bool isAdhesionEnabled() const;

    void setCohesionBindForceStrength(double strength);
    double getCohesionBindForceStrength() const;

    // Viscosity control.
    void setViscosityStrength(double strength);
    double getViscosityStrength() const;

    // Friction control (velocity-dependent viscosity).
    void setFrictionStrength(double strength);
    double getFrictionStrength() const;

    void setCOMCohesionRange(uint32_t range);
    uint32_t getCOMCohesionRange() const;

    // Motion state multiplier calculation (for viscosity and other systems).

    // AIR RESISTANCE CONTROL
    void setAirResistanceEnabled(bool enabled);
    bool isAirResistanceEnabled() const;
    void setAirResistanceStrength(double strength);
    double getAirResistanceStrength() const;

    // COM cohesion mode removed - always uses ORIGINAL implementation

    // GRID MANAGEMENT
    void resizeGrid(uint32_t newWidth, uint32_t newHeight);

    // PERFORMANCE AND DEBUGGING
    void dumpTimerStats() const;
    void markUserInput();
    std::string settingsToString() const;
    Timers& getTimers();
    const Timers& getTimers() const;

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
        0.7; // Minimum adhesion needed for horizontal support

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

    // TODO: These should be part of world event generator.
    void setRainRate(double rate);
    double getRainRate() const;
    void spawnMaterialBall(MaterialType material, uint32_t centerX, uint32_t centerY);
    void setWaterColumnEnabled(bool enabled);
    bool isWaterColumnEnabled() const;
    void setLeftThrowEnabled(bool enabled);
    bool isLeftThrowEnabled() const;
    void setRightThrowEnabled(bool enabled);
    bool isRightThrowEnabled() const;
    void setLowerRightQuadrantEnabled(bool enabled);
    bool isLowerRightQuadrantEnabled() const;

    // World state data - public accessors for Pimpl-stored state.
    WorldData& getData();
    const WorldData& getData() const;

    // Grid cache for debug info access.
    GridOfCells& getGrid();
    const GridOfCells& getGrid() const;

    // Physics settings - public accessors for Pimpl-stored settings.
    PhysicsSettings& getPhysicsSettings();
    const PhysicsSettings& getPhysicsSettings() const;

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
    // INTERNAL IMPLEMENTATION (moved to Pimpl for reduced dependencies)
    // =================================================================

    struct Impl;       // Forward declaration.
    Pimpl<Impl> pImpl; // Pimpl containing calculators and internal state.

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
    void applyCohesionForces(const GridOfCells& grid);
    void applyPressureForces();
    void resolveForces(double deltaTime, const GridOfCells& grid);
    void updateTransfers(double deltaTime);
    void processVelocityLimiting(double deltaTime);
    void processMaterialMoves();
    void setupBoundaryWalls();

    // Coordinate conversion helpers (can be public if needed).
    void pixelToCell(int pixelX, int pixelY, int& cellX, int& cellY) const;
    Vector2i pixelToCell(int pixelX, int pixelY) const;
    bool isValidCell(int x, int y) const;
    bool isValidCell(const Vector2i& pos) const;
};

/**
 * ADL (Argument-Dependent Lookup) functions for nlohmann::json automatic conversion.
 */
void to_json(nlohmann::json& j, World::MotionState state);
void from_json(const nlohmann::json& j, World::MotionState& state);

} // namespace DirtSim
