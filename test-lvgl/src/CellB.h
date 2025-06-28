#pragma once

#include "CellInterface.h"
#include "MaterialType.h"
#include "Vector2d.h"

#include <cstdint>
#include <string>
#include <vector>

#include "lvgl/lvgl.h"

/**
 * \file
 * CellB represents a single cell in the WorldB pure-material physics system.
 * Unlike Cell (mixed dirt/water), CellB contains a single material type with
 * a fill ratio [0,1] indicating how much of the cell is occupied.
 */

class CellB : public CellInterface {
public:
    // Material fill threshold constants.
    static constexpr double MIN_FILL_THRESHOLD = 0.001; // Minimum matter to consider
    static constexpr double MAX_FILL_THRESHOLD = 0.999; // Maximum fill before "full"

    // COM bounds (matches original World system).
    static constexpr double COM_MIN = -1.0;
    static constexpr double COM_MAX = 1.0;

    // Default constructor - creates empty air cell.
    CellB();

    // Constructor with material type and fill ratio.
    CellB(MaterialType type, double fill = 1.0);

    // Destructor - cleanup LVGL canvas.
    ~CellB();

    // Copy constructor and assignment - handle LVGL canvas properly.
    CellB(const CellB& other);
    CellB& operator=(const CellB& other);

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
    const Vector2d& getAccumulatedCohesionForce() const { return accumulated_cohesion_force_; }
    const Vector2d& getAccumulatedAdhesionForce() const { return accumulated_adhesion_force_; }
    const Vector2d& getAccumulatedCOMCohesionForce() const
    {
        return accumulated_com_cohesion_force_;
    }

    // Set accumulated forces (called during physics calculation).
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
    void setVelocity(const Vector2d& velocity)
    {
        velocity_ = velocity;
        markDirty();
    }
    void setVelocity(double x, double y)
    {
        velocity_ = Vector2d(x, y);
        markDirty();
    }

    // Pressure system - separate components only.
    // Consumers must explicitly choose which pressure component they need.
    
    // Separate accessors for pressure components
    double getHydrostaticPressure() const { return hydrostatic_pressure_; }
    void setHydrostaticPressure(double pressure) { hydrostatic_pressure_ = pressure; }
    
    double getDynamicPressure() const { return dynamic_pressure_; }
    void setDynamicPressure(double pressure) { dynamic_pressure_ = pressure; }
    void addDynamicPressure(double pressure) { dynamic_pressure_ += pressure; }

    // Debug visualization for dynamic pressure events.
    void setDebugDynamicPressure(double magnitude)
    {
        debug_dynamic_pressure_ = magnitude;
    }
    double getDebugDynamicPressure() const { return debug_dynamic_pressure_; }

    // Pressure gradient for debug visualization.
    void setPressureGradient(const Vector2d& gradient)
    {
        pressure_gradient_ = gradient;
    }
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

    // Material properties.
    const MaterialProperties& getMaterialProperties() const;

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
    double transferTo(CellB& target, double amount);

    // Physics-aware transfer with boundary crossing information
    double transferToWithPhysics(CellB& target, double amount, const Vector2d& boundary_normal);

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
    // RENDERING
    // =================================================================

    // Main drawing method (called by WorldB::draw)
    void draw(lv_obj_t* parent, uint32_t x, uint32_t y);

    // Separate drawing methods for different modes
    void drawNormal(lv_obj_t* parent, uint32_t x, uint32_t y);
    void drawDebug(lv_obj_t* parent, uint32_t x, uint32_t y);

    // Mark the cell as needing to be redrawn
    void markDirty() override;

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

private:
    MaterialType material_type_; // Type of material in this cell.
    double fill_ratio_;          // How full the cell is [0,1].
    Vector2d com_;               // Center of mass position [-1,1].
    Vector2d velocity_;          // 2D velocity vector.

    double hydrostatic_pressure_; // Pressure from gravity/weight (stable equilibrium).
    double dynamic_pressure_;     // Pressure from blocked transfers (drives flow).
    double debug_dynamic_pressure_; // Debug value for dynamic pressure events.

    Vector2d pressure_gradient_;  // Pressure gradient for debug visualization.

    // Force accumulation for visualization.
    Vector2d accumulated_cohesion_force_;     // Last calculated cohesion force.
    Vector2d accumulated_adhesion_force_;     // Last calculated adhesion force.
    Vector2d accumulated_com_cohesion_force_; // Last calculated COM cohesion force.

    // Physics force accumulation.
    Vector2d pending_force_; // Forces to be applied during resolution phase.

    // Rendering state.
    std::vector<uint8_t> buffer_; // Buffer for LVGL canvas pixel data.
    lv_obj_t* canvas_;            // LVGL canvas object.
    bool needs_redraw_;           // Flag to track if cell needs redrawing.

    // =================================================================
    // PHYSICS HELPERS
    // =================================================================

    // Calculate realistic landing position for transferred material.
    Vector2d calculateTrajectoryLanding(
        const Vector2d& source_com,
        const Vector2d& velocity,
        const Vector2d& boundary_normal) const;
};
