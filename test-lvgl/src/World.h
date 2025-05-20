#pragma once

#include "Cell.h"

#include <cstdint>
#include <vector>

class World {
public:
  World(uint32_t width, uint32_t height, lv_obj_t* draw_area);

  void advanceTime(uint32_t deltaTimeMs);

  void draw();

  void reset();

  Cell &at(uint32_t x, uint32_t y);
  const Cell &at(uint32_t x, uint32_t y) const;

  uint32_t getWidth() const;
  uint32_t getHeight() const;

  void fillWithDirt();
  void makeWalls();

private:
  lv_obj_t* draw_area;

  uint32_t width;
  uint32_t height;
  std::vector<Cell> cells;

  size_t coordToIndex(uint32_t x, uint32_t y) const;
};