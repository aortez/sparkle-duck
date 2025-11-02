#pragma once

#include "CellInterface.h"
#include "MaterialType.h"
#include "Vector2d.h"

#include <cstdint>
#include <string>
#include <vector>

#include "lvgl/lvgl.h"
#include "lvgl/src/libs/thorvg/rapidjson/document.h"

/**
 * \file
 * Cell represents a single cell in the World pure-material physics system.
 * Unlike Cell (mixed dirt/water), Cell contains a single material type with
 * a fill ratio [0,1] indicating how much of the cell is occupied.
 */

class Cell : public CellInterface {
public:
    // Material fill threshold constants.
    static constexpr double MIN_FILL_THRESHOLD = 0.001; // Minimum matter to consider
    static constexpr double MAX_FILL_THRESHOLD = 0.999; // Maximum fill before "full"

    // COM bounds (matches original World system).
    static constexpr double COM_MIN = -1.0;
    static constexpr double COM_MAX = 1.0;

    // Cell rendering dimensions (pixels).
    static constexpr uint32_t WIDTH = 30;
    static constexpr uint32_t HEIGHT = 30;

    // Default constructor - creates empty air cell.
    Cell();

    // Constructor with material type and fill ratio.
    Cell(MaterialType type, double fill = 1.0);

    // Destructor - cleanup LVGL canvas.
    ~Cell();

    // Copy constructor and assignment - handle LVGL canvas properly.
    Cell(const Cell& other);
    Cell& operator=(const Cell& other);

    // =================================================================
    // MATERIAL PROPERTIES
    // =================================================================

    // Get/set material type.
    MaterialType getMaterialType() const { return material_type_; }
    void setMaterialType(MaterialType type) { material_type_ = type; }

    // Get/set fill ratio [0,1].
    double getFillRatio() const { return fill_ratio_; }
    void setFillRatio(double ratio);

    // =================================================================
    // FORCE ACCUMULATION (for visualization)
    // =================================================================

    // Get accumulated forces from last physics calculation.
    // Note: Cohesion force repurposed to store viscosity damping info (X=motion_mult, Y=damping).
    const Vector2d& getAccumulatedCohesionForce() const { return accumulated_cohesion_force_; }
    const Vector2d& getAccumulatedAdhesionForce() const { return accumulated_adhesion_force_; }
    const Vector2d& getAccumulatedCOMCohesionForce() const
    {
        return accumulated_com_cohesion_force_;
    }

    // Set accumulated forces (called during physics calculation).
    // Note: Cohesion force repurposed to store viscosity damping info (X=motion_mult, Y=damping).
    void setAccumulatedCohesionForce(const Vector2d& force) { accumulated_cohesion_force_ = force; }
    void setAccumulatedAdhesionForce(const Vector2d& force) { accumulated_adhesion_force_ = force; }
    void setAccumulatedCOMCohesionForce(const Vector2d& force)
    {
        accumulated_com_cohesion_force_ = force;
    }

    // Clear accumulated forces (for initialization).
    void clearAccumulatedForces()
    {
        accumulated_cohesion_force_ = Vector2d(0, 0);
        accumulated_adhesion_force_ = Vector2d(0, 0);
        accumulated_com_cohesion_force_ = Vector2d(0, 0);
    }

    // Get/set cached friction coefficient for visualization.
    double getCachedFrictionCoefficient() const { return cached_friction_coefficient_; }
    void setCachedFrictionCoefficient(double coeff) { cached_friction_coefficient_ = coeff; }

    // =================================================================
    // PHYSICS FORCE ACCUMULATION
    // =================================================================

    // Get/set pending forces to be applied during force resolution.
    const Vector2d& getPendingForce() const { return pending_force_; }
    void setPendingForce(const Vector2d& force) { pending_force_ = force; }
    void addPendingForce(const Vector2d& force) { pending_force_ = pending_force_ + force; }
    void clearPendingForce() { pending_force_ = Vector2d(0, 0); }

    // Material state queries.
    bool isEmpty() const override { return fill_ratio_ < MIN_FILL_THRESHOLD; }
    bool isFull() const { return fill_ratio_ > MAX_FILL_THRESHOLD; }
    bool isAir() const { return material_type_ == MaterialType::AIR; }
    bool isWall() const { return material_type_ == MaterialType::WALL; }

    // =================================================================
    // PHYSICS PROPERTIES
    // =================================================================

    // Center of mass position [-1,1] within cell.
    const Vector2d& getCOM() const { return com_; }
    void setCOM(const Vector2d& com);
    void setCOM(double x, double y) { setCOM(Vector2d(x, y)); }

    // Velocity vector.
    const Vector2d& getVelocity() const { return velocity_; }
    void setVelocity(const Vector2d& velocity) { velocity_ = velocity; }
    void setVelocity(double x, double y) { velocity_ = Vector2d(x, y); }

    // Unified pressure system with component tracking for debugging.
    // Primary interface - use these for physics calculations.
    double getPressure() const { return pressure_; }
    void setPressure(double pressure) { pressure_ = pressure; }
    void addPressure(double pressure) { pressure_ += pressure; }

    // Legacy interface - now uses components directly.
    // Maintained for backward compatibility.
    double getHydrostaticPressure() const { return hydrostatic_component_; }
    void setHydrostaticPressure(double pressure) { hydrostatic_component_ = pressure; }

    double getDynamicPressure() const { return dynamic_component_; }
    void setDynamicPressure(double pressure) { dynamic_component_ = pressure; }
    void addDynamicPressure(double pressure) { dynamic_component_ += pressure; }

    // Debug/visualization interface - for understanding pressure sources.
    double getHydrostaticComponent() const { return hydrostatic_component_; }
    double getDynamicComponent() const { return dynamic_component_; }
    void setComponents(double hydrostatic, double dynamic)
    {
        hydrostatic_component_ = hydrostatic;
        dynamic_component_ = dynamic;
        // Note: This doesn't update the unified pressure - use for visualization only.
    }

    // Pressure gradient for debug visualization.
    void setPressureGradient(const Vector2d& gradient) { pressure_gradient_ = gradient; }
    const Vector2d& getPressureGradient() const { return pressure_gradient_; }

    // =================================================================
    // CALCULATED PROPERTIES
    // =================================================================

    // Available capacity for more material
    double getCapacity() const { return 1.0 - fill_ratio_; }

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
        const Vector2d& velocity,
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
    void clear() override;

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

    // =================================================================
    // DEBUGGING
    // =================================================================

    // Debug string representation
    std::string toString() const;

    // =================================================================
    // CELLINTERFACE IMPLEMENTATION
    // =================================================================

    // Basic material addition
    void addDirt(double amount) override;
    void addWater(double amount) override;

    // Advanced material addition with physics.
    void addDirtWithVelocity(double amount, const Vector2d& velocity) override;
    void addWaterWithVelocity(double amount, const Vector2d& velocity) override;
    void addDirtWithCOM(double amount, const Vector2d& com, const Vector2d& velocity) override;

    // Cell state management (markDirty declared above in rendering section).

    // Material properties.
    double getTotalMaterial() const override;

    // ASCII visualization.
    std::string toAsciiCharacter() const override;

    // =================================================================
    // JSON SERIALIZATION
    // =================================================================

    // Serialize cell state to JSON.
    rapidjson::Value toJson(rapidjson::Document::AllocatorType& allocator) const;

    // Deserialize cell state from JSON.
    static Cell fromJson(const rapidjson::Value& json);

private:
    MaterialType material_type_; // Type of material in this cell.
    double fill_ratio_;          // How full the cell is [0,1].
    Vector2d com_;               // Center of mass position [-1,1].
    Vector2d velocity_;          // 2D velocity vector.

    // Unified pressure system.
    double pressure_;              // Total pressure for physics calculations.
    double hydrostatic_component_; // Hydrostatic contribution (for debugging/visualization).
    double dynamic_component_;     // Dynamic contribution (for debugging/visualization).

    Vector2d pressure_gradient_; // Pressure gradient for debug visualization.

    // Force accumulation for visualization.
    Vector2d accumulated_cohesion_force_;     // Repurposed: X=motion_multiplier, Y=damping_factor.
    Vector2d accumulated_adhesion_force_;     // Last calculated adhesion force.
    Vector2d accumulated_com_cohesion_force_; // Last calculated COM cohesion force.

    // Physics force accumulation.
    Vector2d pending_force_; // Forces to be applied during resolution phase.

    // Cached physics values for visualization.
    double cached_friction_coefficient_;

    // =================================================================
    // PHYSICS HELPERS
    // =================================================================

    // Calculate realistic landing position for transferred material.
    Vector2d calculateTrajectoryLanding(
        const Vector2d& source_com,
        const Vector2d& velocity,
        const Vector2d& boundary_normal) const;

    // Helper to update unified pressure from components.
    void updateUnifiedPressure() { pressure_ = hydrostatic_component_ + dynamic_component_; }
};
