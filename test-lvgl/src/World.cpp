#include "World.h"

#include <stdexcept>

World::World(uint32_t width, uint32_t height) 
    : width(width), height(height), cells(width * height) {
}

void World::advanceTime(uint32_t deltaTimeMs) {
    // TODO: Implement time advancement logic
}

Cell& World::at(uint32_t x, uint32_t y) {
    if (x >= width || y >= height) {
        throw std::out_of_range("World::at: Coordinates out of range");
    }
    return cells[coordToIndex(x, y)];
}

const Cell& World::at(uint32_t x, uint32_t y) const {
    if (x >= width || y >= height) {
        throw std::out_of_range("World::at: Coordinates out of range");
    }
    return cells[coordToIndex(x, y)];
}

size_t World::coordToIndex(uint32_t x, uint32_t y) const {
    return y * width + x;
}

uint32_t World::getWidth() const {
    return width;
}

uint32_t World::getHeight() const {
    return height;
}

void World::makeWalls() {
    for (uint32_t x = 0; x < width; x++) {
        at(x, 0).dirty = 1.0;
        at(x, height - 1).dirty = 1.0;
    }
    for (uint32_t y = 0; y < height; y++) {
        at(0, y).dirty = 1.0;
        at(width - 1, y).dirty = 1.0;
    }
}

void World::reset() {
    cells.clear();
    cells.resize(width * height);
}

