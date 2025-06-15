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
    // Material fill threshold constants
    static constexpr double MIN_FILL_THRESHOLD = 0.001;  // Minimum matter to consider
    static constexpr double MAX_FILL_THRESHOLD = 0.999;  // Maximum fill before "full"
    
    // COM bounds (matches original World system)
    static constexpr double COM_MIN = -1.0;
    static constexpr double COM_MAX = 1.0;
    
    // Default constructor - creates empty air cell
    CellB();
    
    // Constructor with material type and fill ratio
    CellB(MaterialType type, double fill = 1.0);
    
    // Destructor - cleanup LVGL canvas
    ~CellB();
    
    // Copy constructor and assignment - handle LVGL canvas properly
    CellB(const CellB& other);
    CellB& operator=(const CellB& other);
    
    // =================================================================
    // MATERIAL PROPERTIES
    // =================================================================
    
    // Get/set material type
    MaterialType getMaterialType() const { return material_type_; }
    void setMaterialType(MaterialType type) { material_type_ = type; }
    
    // Get/set fill ratio [0,1]
    double getFillRatio() const { return fill_ratio_; }
    void setFillRatio(double ratio);
    
    // Material state queries
    bool isEmpty() const override { return fill_ratio_ < MIN_FILL_THRESHOLD; }
    bool isFull() const { return fill_ratio_ > MAX_FILL_THRESHOLD; }
    bool isAir() const { return material_type_ == MaterialType::AIR; }
    bool isWall() const { return material_type_ == MaterialType::WALL; }
    
    // =================================================================
    // PHYSICS PROPERTIES
    // =================================================================
    
    // Center of mass position [-1,1] within cell
    const Vector2d& getCOM() const { return com_; }
    void setCOM(const Vector2d& com);
    void setCOM(double x, double y) { setCOM(Vector2d(x, y)); }
    
    // Velocity vector
    const Vector2d& getVelocity() const { return velocity_; }
    void setVelocity(const Vector2d& velocity) { velocity_ = velocity; markDirty(); }
    void setVelocity(double x, double y) { velocity_ = Vector2d(x, y); markDirty(); }
    
    // Pressure (hydrostatic)
    double getPressure() const { return pressure_; }
    void setPressure(double pressure) { pressure_ = pressure; }
    
    // =================================================================
    // CALCULATED PROPERTIES
    // =================================================================
    
    // Available capacity for more material
    double getCapacity() const { return 1.0 - fill_ratio_; }
    
    // Effective mass (fill_ratio * material_density)
    double getMass() const;
    
    // Effective density
    double getEffectiveDensity() const;
    
    // Material properties
    const MaterialProperties& getMaterialProperties() const;
    
    // =================================================================
    // MATERIAL OPERATIONS
    // =================================================================
    
    // Add material to this cell (returns amount actually added)
    double addMaterial(MaterialType type, double amount);
    
    // Add material with physics context for realistic COM placement
    double addMaterialWithPhysics(MaterialType type, double amount, 
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
    
    // Apply velocity limiting per GridMechanics.md
    void limitVelocity(double max_velocity, double damping_threshold, double damping_factor);
    
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
    
    // Advanced material addition with physics
    void addDirtWithVelocity(double amount, const Vector2d& velocity) override;
    void addWaterWithVelocity(double amount, const Vector2d& velocity) override;
    void addDirtWithCOM(double amount, const Vector2d& com, const Vector2d& velocity) override;
    
    // Cell state management (markDirty declared above in rendering section)
    
    // Material properties
    double getTotalMaterial() const override;

private:
    MaterialType material_type_;  // Type of material in this cell
    double fill_ratio_;          // How full the cell is [0,1]
    Vector2d com_;               // Center of mass position [-1,1]
    Vector2d velocity_;          // 2D velocity vector
    double pressure_;            // Hydrostatic pressure
    
    // Rendering state
    std::vector<uint8_t> buffer_; // Buffer for LVGL canvas pixel data
    lv_obj_t* canvas_;           // LVGL canvas object
    bool needs_redraw_;          // Flag to track if cell needs redrawing
    
    // =================================================================
    // PHYSICS HELPERS
    // =================================================================
    
    // Calculate realistic landing position for transferred material
    Vector2d calculateTrajectoryLanding(const Vector2d& source_com, 
                                       const Vector2d& velocity,
                                       const Vector2d& boundary_normal) const;
};