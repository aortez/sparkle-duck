#include "Cell.h"

#include <cstring>
#include <string>

#include "lvgl/lvgl.h"

Cell::Cell() : dirty(0.0), buffer(), canvas(nullptr), com(0.0, 0.0), v(0.0, 0.0)
{}

// Helper function to safely set a pixel on the canvas.
static void safe_set_pixel(lv_obj_t* canvas, int x, int y, lv_color_t color, lv_opa_t opa)
{
    if (x >= 0 && x < Cell::WIDTH && y >= 0 && y < Cell::HEIGHT) {
        lv_canvas_set_px(canvas, x, y, color, opa);
    }
}

void Cell::draw(lv_obj_t* parent, uint32_t x, uint32_t y)
{
    if (canvas == nullptr) {
        canvas = lv_canvas_create(parent);
        lv_obj_set_size(canvas, Cell::WIDTH, Cell::HEIGHT);
        lv_obj_set_pos(canvas, x * Cell::WIDTH, y * Cell::HEIGHT);

        lv_canvas_set_buffer(
            canvas, buffer.data(), Cell::WIDTH, Cell::HEIGHT, LV_COLOR_FORMAT_ARGB8888);
    }

    lv_color_t black = lv_color_hex(0x000000);
    lv_color_t brown = lv_color_hex(0x8B4513); // Saddle brown color
    lv_color_t yellow = lv_color_hex(0xFFFF00);

    // Zero buffer.
    std::fill(buffer.begin(), buffer.end(), 0);

    // Calculate opacity based on dirt amount (0.0 to 1.0)
    lv_opa_t opacity = static_cast<lv_opa_t>(dirty * LV_OPA_COVER);

    // Fill each pixel with brown color, using opacity based on dirt amount.
    for (int y = 1; y < HEIGHT - 1; y++) {
        for (int x = 1; x < WIDTH - 1; x++) {
            safe_set_pixel(canvas, x, y, brown, opacity);
        }
    }

    // Draw a cross at the center of mass.
    // First, scale the center of mass to the range [0,CELL_WIDTH/HEIGHT].
    int pixel_x = static_cast<int>((com.x + 1.0) * (WIDTH - 1) / 2.0);
    int pixel_y = static_cast<int>((com.y + 1.0) * (HEIGHT - 1) / 2.0);

    // Draw a white cross at the center of mass with black border.
    lv_color_t white = lv_color_hex(0xFFFFFF);

    // Draw black border first.
    // Horizontal line border.
    for (int x = -3; x <= 3; x++) {
        safe_set_pixel(canvas, pixel_x + x, pixel_y - 1, black, LV_OPA_COVER);
        safe_set_pixel(canvas, pixel_x + x, pixel_y + 1, black, LV_OPA_COVER);
    }
    // Vertical line border.
    for (int y = -3; y <= 3; y++) {
        safe_set_pixel(canvas, pixel_x - 1, pixel_y + y, black, LV_OPA_COVER);
        safe_set_pixel(canvas, pixel_x + 1, pixel_y + y, black, LV_OPA_COVER);
    }

    // Draw white cross on top.
    // Horizontal line.
    for (int x = -2; x <= 2; x++) {
        safe_set_pixel(canvas, pixel_x + x, pixel_y, white, LV_OPA_COVER);
    }
    // Vertical line.
    for (int y = -2; y <= 2; y++) {
        safe_set_pixel(canvas, pixel_x, pixel_y + y, white, LV_OPA_COVER);
    }

    // Draw border around the cell.
    // Draw top and bottom borders.
    for (int i = 0; i < WIDTH; i++) {
        safe_set_pixel(canvas, i, 0, yellow, LV_OPA_COVER);          // Top border.
        safe_set_pixel(canvas, i, HEIGHT - 1, yellow, LV_OPA_COVER); // Bottom border.
    }

    // Draw left and right borders.
    for (int i = 0; i < HEIGHT; i++) {
        safe_set_pixel(canvas, 0, i, yellow, LV_OPA_COVER);         // Left border.
        safe_set_pixel(canvas, WIDTH - 1, i, yellow, LV_OPA_COVER); // Right border.
    }
}

std::string Cell::toString() const
{
    return "Cell{dirty=" + std::to_string(dirty) + ", com=" + com.toString() + ", v=" + v.toString()
        + "}";
}
