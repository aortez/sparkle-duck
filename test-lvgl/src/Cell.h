#pragma once

#include "Vector2d.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

// Forward declare LVGL types.
typedef struct _lv_obj_t lv_obj_t;

// Forward declare World class
class World;

// A cell in grid-based simulation.
class Cell {
public:
    static bool debugDraw;
    static uint32_t WIDTH;
    static uint32_t HEIGHT;

    // COM deflection threshold for triggering transfers
    static constexpr double COM_DEFLECTION_THRESHOLD = 0.6;

    // Water physics constants - now configurable
    static double COHESION_STRENGTH;
    static double VISCOSITY_FACTOR;
    static double BUOYANCY_STRENGTH;

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
    void markDirty();

    // Update cell properties and mark dirty
    void update(double newDirty, const Vector2d& newCom, const Vector2d& newV);

    // Calculate total percentage of cell filled with elements
    double percentFull() const { return dirt + water + wood + leaf + metal; }

    // Get normalized COM deflection in range [-1, 1]
    // Returns COM normalized by the deflection threshold
    Vector2d getNormalizedDeflection() const;

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

    Vector2d calculateBuoyancy(const Cell& cell, const Cell& neighbor, int dx, int dy) const;

private:
    std::vector<uint8_t> buffer; // Use vector instead of array for dynamic sizing

    lv_obj_t* canvas;
    bool needsRedraw = true; // Flag to track if cell needs redrawing
};
