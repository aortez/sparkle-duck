#pragma once

#include "Vector2d.h"

#include <array>
#include <cstdint>
#include <string>

// Forward declare LVGL types.
typedef struct _lv_obj_t lv_obj_t;

class Cell {
public:
    static const int WIDTH = 30;
    static const int HEIGHT = 30;

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

    static bool debugDraw; // If true, draw in debug mode

    Cell();
    std::string toString() const;

    Vector2d calculateWaterCohesion(const Cell& cell, const Cell& neighbor) const;

    void applyViscosity(const Cell& neighbor);

private:
    std::array<uint8_t, WIDTH * HEIGHT * 4> buffer;

    lv_obj_t* canvas;
    bool needsRedraw = true; // Flag to track if cell needs redrawing
};
