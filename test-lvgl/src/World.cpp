#include "World.h"

#include <algorithm>
#include <stdexcept>

World::World(uint32_t width, uint32_t height, lv_obj_t* draw_area)
    : width(width), height(height), cells(width * height), draw_area(draw_area)
{}

void World::advanceTime(uint32_t deltaTimeMs)
{
    const double gravity = 9.81;                  // m/s^2.
    const double timeStep = deltaTimeMs / 1000.0; // Convert to seconds.

    for (uint32_t y = 0; y < height; y++) {
        for (uint32_t x = 0; x < width; x++) {
            Cell& cell = at(x, y);

            // Apply gravity to velocity.
            cell.v.y += gravity * timeStep;

            // Update center of mass based on velocity.
            cell.com += cell.v * timeStep;
        }
    }
}

Cell& World::at(uint32_t x, uint32_t y)
{
    if (x >= width || y >= height) {
        throw std::out_of_range("World::at: Coordinates out of range");
    }
    return cells[coordToIndex(x, y)];
}

const Cell& World::at(uint32_t x, uint32_t y) const
{
    if (x >= width || y >= height) {
        throw std::out_of_range("World::at: Coordinates out of range");
    }
    return cells[coordToIndex(x, y)];
}

size_t World::coordToIndex(uint32_t x, uint32_t y) const
{
    return y * width + x;
}

void World::draw()
{
    if (draw_area == nullptr) {
        throw std::runtime_error("World::draw: Parent is not set");
    }

    for (uint32_t y = 0; y < height; y++) {
        for (uint32_t x = 0; x < width; x++) {
            at(x, y).draw(draw_area, x, y);
        }
    }
}

uint32_t World::getWidth() const
{
    return width;
}

uint32_t World::getHeight() const
{
    return height;
}

void World::fillWithDirt()
{
    for (uint32_t y = 0; y < height; y++) {
        for (uint32_t x = 0; x < width; x++) {
            at(x, y).dirty = 0.5;
            at(x, y).v = Vector2d(1, 0.0);
        }
    }
}

void World::makeWalls()
{
    for (uint32_t x = 0; x < width; x++) {
        at(x, 0).dirty = 1.0;
        at(x, height - 1).dirty = 1.0;
    }
    for (uint32_t y = 0; y < height; y++) {
        at(0, y).dirty = 1.0;
        at(width - 1, y).dirty = 1.0;
    }
}

void World::reset()
{
    cells.clear();
    cells.resize(width * height);
}
