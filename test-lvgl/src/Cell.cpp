#include "Cell.h"

#include <cstring>
#include <string>

#include "lvgl/lvgl.h"

Cell::Cell()
    : dirty(0.0), buffer(), canvas(nullptr), com(0.0, 0.0), v(0.0, 0.0) {}

void Cell::draw(lv_obj_t *parent, uint32_t x, uint32_t y) {
  if (canvas == nullptr) {
    canvas = lv_canvas_create(parent);
    lv_obj_set_size(canvas, Cell::WIDTH, Cell::HEIGHT);
    lv_obj_set_pos(canvas, x * Cell::WIDTH, y * Cell::HEIGHT);

    lv_canvas_set_buffer(canvas, buffer.data(), Cell::WIDTH, Cell::HEIGHT,
                         LV_COLOR_FORMAT_ARGB8888);
  }

  // Zero buffer.
  std::fill(buffer.begin(), buffer.end(), 0);

  // Draw a single pixel in the center of mass.
  // First, scale the center of mass to the range [0,CELL_WIDTH/HEIGHT].
  int pixel_x = static_cast<int>((com.x + 1.0) * (WIDTH - 1) / 2.0);
  int pixel_y = static_cast<int>((com.y + 1.0) * (HEIGHT - 1) / 2.0);

  // Draw a blue pixel at the center of mass.
  lv_canvas_set_px(canvas, pixel_x, pixel_y, lv_color_hex(0x0000ff),
                   LV_OPA_COVER);

  // Draw black border around the cell.
  lv_color_t black = lv_color_hex(0x000000);

  // Draw top and bottom borders.
  for (int i = 0; i < WIDTH; i++) {
    lv_canvas_set_px(canvas, i, 0, black, LV_OPA_COVER); // Top border.
    lv_canvas_set_px(canvas, i, HEIGHT - 1, black,
                     LV_OPA_COVER); // Bottom border.
  }

  // Draw left and right borders.
  for (int i = 0; i < HEIGHT; i++) {
    lv_canvas_set_px(canvas, 0, i, black, LV_OPA_COVER); // Left border.
    lv_canvas_set_px(canvas, WIDTH - 1, i, black,
                     LV_OPA_COVER); // Right border.
  }
}

std::string Cell::toString() const {
  return "Cell{dirty=" + std::to_string(dirty) + ", com=" + com.toString() +
         ", v=" + v.toString() + "}";
}
