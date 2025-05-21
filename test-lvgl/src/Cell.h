#pragma once

#include "Vector2d.h"
#include "lvgl/lvgl.h"

#include <array>
#include <string>

class Cell {
public:
static const int WIDTH = 100;
static const int HEIGHT = 100;

  void draw(lv_obj_t *parent, uint32_t x, uint32_t y);

  // Amount of dirt in cell [0,1].
  double dirty;

  // Center of mass of dirt, range [-1,1].
  Vector2d com;

  // Velocity of dirt.
  Vector2d v;

  Cell();
  std::string toString() const;

private:
  std::array<uint8_t, WIDTH * HEIGHT * 4> buffer;

  lv_obj_t *canvas;
};
