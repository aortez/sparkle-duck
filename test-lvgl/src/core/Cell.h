#pragma once

#include "MaterialType.h"
#include "Vector2d.h"

#include <cstdint>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace DirtSim {

/**
 * \file
 * Cell represents a single cell in the World pure-material physics system.
 * Unlike Cell (mixed dirt/water), Cell contains a single material type with
 * a fill ratio [0,1] indicating how much of the cell is occupied.
 *
 * Note: Direct member access is now public. Use helper methods when invariants matter.
 */

struct Cell {
    // Material fill threshold constants.
    static constexpr double MIN_FILL_THRESHOLD = 0.001; // Minimum matter to consider
    static constexpr double MAX_FILL_THRESHOLD = 0.999; // Maximum fill before "full"

    // COM bounds.
    static constexpr double COM_MIN = -1.0;
    static constexpr double COM_MAX = 1.0;

    // Cell rendering dimensions (pixels).
    static constexpr uint32_t WIDTH = 30;
    static constexpr uint32_t HEIGHT = 30;

    // =================================================================
    // PUBLIC DATA MEMBERS (aggregate type)
    // =================================================================

    MaterialType material_type = MaterialType::AIR;
    double fill_ratio = 0.0;
    Vector2d com = {};
    Vector2d velocity = {};
    uint32_t organism_id = 0; // Tree organism ownership (0 = no organism).

    // Unified pressure system.
    double pressure = 0.0;
    double hydrostatic_component = 0.0;
    double dynamic_component = 0.0;

    Vector2d pressure_gradient = {};

    // Force accumulation for visualization.
    Vector2d accumulated_viscous_force = {};      // Viscous force from momentum diffusion.
    Vector2d accumulated_adhesion_force = {};     // Adhesion force (different materials).
    Vector2d accumulated_com_cohesion_force = {}; // COM cohesion force (same material).

    // Physics force accumulation.
    Vector2d pending_force = {};

    // Cached physics values for visualization.
    double cached_friction_coefficient = 1.0;

    // Computed structural support (updated each frame).
    bool has_any_support = false;
    bool has_vertical_support = false;

    // =================================================================
    // MATERIAL PROPERTIES
    // =================================================================

    const MaterialProperties& material() const;

    // Helper with invariant: clamps fill ratio and auto-converts to AIR.
    void setFillRatio(double ratio);

    // =================================================================
    // FORCE ACCUMULATION (for visualization)
    // =================================================================

    void clearAccumulatedForces();

    // =================================================================
    // PHYSICS FORCE ACCUMULATION
    // =================================================================

    // Helper to add and clear pending forces.
    void addPendingForce(const Vector2d& force);
    void clearPendingForce();

    // Convenience queries.
    bool isEmpty() const;
    bool isFull() const;
    bool isAir() const;
    bool isWall() const;

    // =================================================================
    // PHYSICS PROPERTIES
    // =================================================================

    // Center of mass position [-1,1] within cell (has clamping logic).
    void setCOM(const Vector2d& com);
    void setCOM(double x, double y);

    // Helpers with logic for pressure component management.
    void setHydrostaticPressure(double p);
    void setDynamicPressure(double p);
    void addDynamicPressure(double p);
    void clearPressure();

    // =================================================================
    // CALCULATED PROPERTIES
    // =================================================================

    // Available capacity for more material.
    double getCapacity() const;

    // Effective mass (fill_ratio * material_density)
    double getMass() const;

    // Effective density
    double getEffectiveDensity() const;

    // =================================================================
    // MATERIAL OPERATIONS
    // =================================================================

    // Add material to this cell (returns amount actually added)
    double addMaterial(MaterialType type, double amount);

    // Add material with physics context for realistic COM placement
    double addMaterialWithPhysics(
        MaterialType type,
        double amount,
        const Vector2d& source_com,
        const Vector2d& newVel,
        const Vector2d& boundary_normal);

    // Remove material from this cell (returns amount actually removed)
    double removeMaterial(double amount);

    // Transfer material to another cell (returns amount transferred)
    double transferTo(Cell& target, double amount);

    // Physics-aware transfer with boundary crossing information
    double transferToWithPhysics(Cell& target, double amount, const Vector2d& boundary_normal);

    // Replace all material with new type and amount
    void replaceMaterial(MaterialType type, double fill_ratio = 1.0);

    // Clear cell (set to empty air)
    void clear();

    // =================================================================
    // PHYSICS UTILITIES
    // =================================================================

    // Apply velocity limiting per GridMechanics.md (per-timestep values)
    void limitVelocity(
        double max_velocity_per_timestep,
        double damping_threshold_per_timestep,
        double damping_factor_per_timestep,
        double deltaTime);

    // Clamp COM to valid bounds
    void clampCOM();

    // Check if COM indicates transfer should occur
    bool shouldTransfer() const;

    // Get transfer direction based on COM position
    Vector2d getTransferDirection() const;

    // =================================================================
    // CELLINTERFACE IMPLEMENTATION
    // =================================================================

    // Basic material addition
    void addDirt(double amount);
    void addWater(double amount);

    // Advanced material addition with physics.
    void addDirtWithVelocity(double amount, const Vector2d& newVel);
    void addDirtWithCOM(double amount, const Vector2d& newCom, const Vector2d& newVel);

    // Material properties.
    double getTotalMaterial() const;

    // ASCII visualization.
    std::string toAsciiCharacter() const;

    // =================================================================
    // JSON SERIALIZATION
    // =================================================================

    // Debug string representation
    std::string toString() const;

    nlohmann::json toJson() const;
    static Cell fromJson(const nlohmann::json& json);

    // =================================================================
    // HELPER METHODS
    // =================================================================

    // Calculate realistic landing position for transferred material.
    Vector2d calculateTrajectoryLanding(
        const Vector2d& source_com,
        const Vector2d& velocity,
        const Vector2d& boundary_normal) const;

    // Helper to update unified pressure from components.
    void updateUnifiedPressure();
};

/**
 * ADL (Argument-Dependent Lookup) functions for nlohmann::json automatic conversion.
 */
inline void to_json(nlohmann::json& j, const Cell& cell)
{
    j = cell.toJson();
}

inline void from_json(const nlohmann::json& j, Cell& cell)
{
    cell = Cell::fromJson(j);
}

} // namespace DirtSim
