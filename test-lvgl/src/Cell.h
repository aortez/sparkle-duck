#pragma once

#include "Vector2d.h"

#include <string>

class Cell {
public:
    // Amount of dirt in cell [0,1].
    double dirty;
    
    // Center of mass of dirt [-1,1] in both x and y.
    Vector2d com;
    
    // Velocity of dirt.
    Vector2d v;

    Cell();
    std::string toString() const;
}; 