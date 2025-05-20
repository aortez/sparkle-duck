#pragma once

#include "Cell.h"

#include <vector>
#include <cstdint>

class World {
public:
    World(uint32_t width, uint32_t height);
    
    void advanceTime(uint32_t deltaTimeMs);
    
    void reset();
    
    Cell& at(uint32_t x, uint32_t y);
    const Cell& at(uint32_t x, uint32_t y) const;
    
    uint32_t getWidth() const;
    uint32_t getHeight() const;

    void makeWalls();

private:
    uint32_t width;
    uint32_t height;
    std::vector<Cell> cells;
    
    size_t coordToIndex(uint32_t x, uint32_t y) const;  
};