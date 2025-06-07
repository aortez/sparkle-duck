#pragma once

#include "Vector2d.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

// Forward declare LVGL types.
typedef struct _lv_obj_t lv_obj_t;

// A cell in grid-based simulation.
class Cell {
public:
    static bool debugDraw;
    static uint32_t WIDTH;
    static uint32_t HEIGHT;

    static void setSize(uint32_t newSize)
    {
        WIDTH = newSize;
        HEIGHT = newSize;
    }

    static uint32_t getSize() { return WIDTH; }

    void draw(lv_obj_t* parent, uint32_t x, uint32_t y);

    // Mark the cell as needing to be redrawn
    void markDirty();

    // Update cell properties and mark dirty
    void update(double newDirty, const Vector2d& newCom, const Vector2d& newV);

    // Calculate total percentage of cell filled with elements
    double percentFull() const { return dirt + water + wood + leaf + metal; }

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
    std::string toString() const;

    Vector2d calculateWaterCohesion(const Cell& cell, const Cell& neighbor) const;

    void applyViscosity(const Cell& neighbor);

private:
    std::vector<uint8_t> buffer; // Use vector instead of array for dynamic sizing

    lv_obj_t* canvas;
    bool needsRedraw = true; // Flag to track if cell needs redrawing
};
