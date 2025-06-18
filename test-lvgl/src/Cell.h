#pragma once

#include "CellInterface.h"
#include "Vector2d.h"
#include "Vector2i.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

// Forward declare LVGL types.
typedef struct _lv_obj_t lv_obj_t;

// Forward declare World class
class World;

// A cell in grid-based simulation.
class Cell : public CellInterface {
public:
    static bool debugDraw;
    static bool adhesionDrawEnabled;
    static uint32_t WIDTH;
    static uint32_t HEIGHT;

    // COM deflection threshold for triggering transfers
    static constexpr double COM_DEFLECTION_THRESHOLD = 0.6;

    // Water physics constants - now configurable
    static double COHESION_STRENGTH;
    static double VISCOSITY_FACTOR;
    static double BUOYANCY_STRENGTH;

    // Density constants for different materials (moderate differences for stability)
    static constexpr double DIRT_DENSITY = 1.3;  // Slightly denser than water
    static constexpr double WATER_DENSITY = 1.0; // Reference density
    static constexpr double WOOD_DENSITY = 0.8;  // Wood floats
    static constexpr double LEAF_DENSITY = 0.7;  // Leaves float
    static constexpr double METAL_DENSITY = 2.0; // Moderately heavy

    // Setters for water physics constants
    static void setCohesionStrength(double strength) { COHESION_STRENGTH = strength; }
    static void setViscosityFactor(double factor) { VISCOSITY_FACTOR = factor; }
    static void setBuoyancyStrength(double strength) { BUOYANCY_STRENGTH = strength; }

    // Getters for water physics constants
    static double getCohesionStrength() { return COHESION_STRENGTH; }
    static double getViscosityFactor() { return VISCOSITY_FACTOR; }
    static double getBuoyancyStrength() { return BUOYANCY_STRENGTH; }

    static void setSize(uint32_t newSize)
    {
        WIDTH = newSize;
        HEIGHT = newSize;
    }

    static uint32_t getSize() { return WIDTH; }

    void draw(lv_obj_t* parent, uint32_t x, uint32_t y);

    // Separate drawing methods for different modes
    void drawNormal(lv_obj_t* parent, uint32_t x, uint32_t y);
    void drawDebug(lv_obj_t* parent, uint32_t x, uint32_t y);

    // Mark the cell as needing to be redrawn
    void markDirty() override;

    // Update cell properties and mark dirty
    void update(double newDirty, const Vector2d& newCom, const Vector2d& newV);

    // Calculate total percentage of cell filled with elements
    double percentFull() const
    {
        double total = dirt + water + wood + leaf + metal;
        return total;
    }

    // Safe version that clamps overfill and logs warnings
    double safePercentFull() const
    {
        double total = percentFull();
        if (total > 1.10) {
            // Log the overfill for debugging - but don't crash
            // Use std::cout since LOG_DEBUG might not be available everywhere
            return 1.10; // Clamp to safe limit
        }
        return total;
    }

    // Method to safely add material while respecting capacity
    // Returns the actual amount added (may be less than requested)
    double safeAddMaterial(double& material, double amount, double maxCapacity = 1.10)
    {
        // Calculate current total WITHOUT the material we're about to modify
        double currentTotal = percentFull() - material;
        double availableSpace = maxCapacity - currentTotal;
        double actualAmount = std::min(amount, std::max(0.0, availableSpace));
        material += actualAmount;
        return actualAmount;
    }

    // Get normalized COM deflection in range [-1, 1]
    // Returns COM normalized by the deflection threshold
    Vector2d getNormalizedDeflection() const;

    // Calculate effective density based on material composition
    double getEffectiveDensity() const;

    // Validate cell state for debugging
    void validateState(const std::string& context) const;

    // Element amounts in cell [0,1]
    double dirt;
    double water;
    double wood;
    double leaf;
    double metal;

    // Center of mass of elements, range [-1,1].
    Vector2d com;

    // Velocity of elements.
    Vector2d v;

    // Pressure force vector
    Vector2d pressure;

    Cell();
    ~Cell(); // Add destructor to clean up buffer

    // Copy constructor and assignment operator to handle LVGL objects properly
    Cell(const Cell& other);
    Cell& operator=(const Cell& other);

    std::string toString() const;

    Vector2d calculateWaterCohesion(
        const Cell& cell,
        const Cell& neighbor,
        const class World* world,
        uint32_t cellX,
        uint32_t cellY) const;

    void applyViscosity(const Cell& neighbor);

    Vector2d calculateBuoyancy(const Cell& cell, const Cell& neighbor, const Vector2i& offset) const;

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
    
    // Cell state management (markDirty already declared above with override)
    void clear() override;
    
    // Material properties
    double getTotalMaterial() const override;
    bool isEmpty() const override;
    
    // ASCII visualization
    std::string toAsciiCharacter() const override;

private:
    std::vector<uint8_t> buffer; // Use vector instead of array for dynamic sizing

    lv_obj_t* canvas;
    bool needsRedraw = true; // Flag to track if cell needs redrawing
};
