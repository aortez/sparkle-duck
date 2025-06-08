#include "Cell.h"

#include "World.h"

#include <cstdio> // For snprintf
#include <cstring>
#include <string>

#include "lvgl/lvgl.h"

namespace {
// Drawing constants.
constexpr double COM_VISUALIZATION_RADIUS = 3.0;
constexpr int VELOCITY_VISUALIZATION_SCALE = 1;
constexpr int PRESSURE_VISUALIZATION_SCALE = 1000;
} // namespace

bool Cell::debugDraw = true;
uint32_t Cell::WIDTH = 50;  // Default size
uint32_t Cell::HEIGHT = 50; // Default size

Cell::Cell()
    : dirt(0.0),
      water(0.0),
      wood(0.0),
      leaf(0.0),
      metal(0.0),
      buffer(WIDTH * HEIGHT * 4),
      canvas(nullptr),
      com(0.0, 0.0),
      v(0.0, 0.0),
      pressure(0.0, 0.0),
      needsRedraw(true)
{}

Cell::~Cell() = default;

void Cell::update(double newDirty, const Vector2d& newCom, const Vector2d& newV)
{
    dirt = newDirty;
    com = newCom;
    v = newV;
    needsRedraw = true;
}

// Helper function to safely set a pixel on the canvas.
static void safe_set_pixel(lv_obj_t* canvas, int x, int y, lv_color_t color, lv_opa_t opa)
{
    if (x >= 0 && x < Cell::WIDTH && y >= 0 && y < Cell::HEIGHT) {
        lv_canvas_set_px(canvas, x, y, color, opa);
    }
}

void Cell::draw(lv_obj_t* parent, uint32_t x, uint32_t y)
{
    // Skip drawing if nothing has changed and canvas exists
    if (!needsRedraw && canvas != nullptr) {
        return;
    }

    if (canvas == nullptr) {
        canvas = lv_canvas_create(parent);
        lv_obj_set_size(canvas, Cell::WIDTH, Cell::HEIGHT);
        lv_obj_set_pos(canvas, x * Cell::WIDTH, y * Cell::HEIGHT);

        lv_canvas_set_buffer(
            canvas, buffer.data(), Cell::WIDTH, Cell::HEIGHT, LV_COLOR_FORMAT_ARGB8888);
    }

    // Zero buffer
    std::fill(buffer.begin(), buffer.end(), 0);

    if (!debugDraw) {
        drawNormal(parent, x, y);
    }
    else {
        drawDebug(parent, x, y);
    }

    // Mark that we've drawn the cell.
    needsRedraw = false;
}

void Cell::drawNormal(lv_obj_t* parent, uint32_t x, uint32_t y)
{
    lv_color_t brown = lv_color_hex(0x8B4513); // Saddle brown color

    // Calculate opacity based on dirt amount (0.0 to 1.0)
    lv_opa_t opacity_dirt = static_cast<lv_opa_t>(dirt * LV_OPA_COVER);
    lv_opa_t opacity_water = static_cast<lv_opa_t>(water * LV_OPA_COVER);

    lv_layer_t layer;
    lv_canvas_init_layer(canvas, &layer);

    // Draw dirt layer
    lv_draw_rect_dsc_t rect_dsc;
    lv_draw_rect_dsc_init(&rect_dsc);
    rect_dsc.bg_color = brown;
    rect_dsc.bg_opa = opacity_dirt;
    rect_dsc.border_color = lv_color_hex(0x000000);
    rect_dsc.border_width = 1;
    rect_dsc.radius = 1;
    lv_area_t coords = { 0, 0, WIDTH, HEIGHT };
    lv_draw_rect(&layer, &rect_dsc, &coords);

    // Draw water layer on top
    lv_draw_rect_dsc_t rect_dsc_water;
    lv_draw_rect_dsc_init(&rect_dsc_water);
    rect_dsc_water.bg_color = lv_color_hex(0x0000FF);
    rect_dsc_water.bg_opa = opacity_water;
    rect_dsc_water.border_color = lv_color_hex(0x000000);
    rect_dsc_water.border_width = 1;
    rect_dsc_water.radius = 1;
    lv_draw_rect(&layer, &rect_dsc_water, &coords);

    lv_canvas_finish_layer(canvas, &layer);
}

void Cell::drawDebug(lv_obj_t* parent, uint32_t x, uint32_t y)
{
    lv_color_t brown = lv_color_hex(0x8B4513); // Saddle brown color

    // Calculate opacity based on dirt amount (0.0 to 1.0)
    lv_opa_t opacity_dirt = static_cast<lv_opa_t>(dirt * LV_OPA_COVER);
    lv_opa_t opacity_water = static_cast<lv_opa_t>(water * LV_OPA_COVER);

    lv_layer_t layer;
    lv_canvas_init_layer(canvas, &layer);

    // Draw dirt background
    lv_draw_rect_dsc_t rect_dsc;
    lv_draw_rect_dsc_init(&rect_dsc);
    rect_dsc.bg_color = brown;
    rect_dsc.bg_opa = opacity_dirt;
    rect_dsc.border_color = lv_color_hex(0x000000);
    rect_dsc.border_width = 1;
    rect_dsc.radius = 1;
    lv_area_t coords = { 0, 0, WIDTH, HEIGHT };
    lv_draw_rect(&layer, &rect_dsc, &coords);

    // Draw water layer
    lv_draw_rect_dsc_t rect_dsc_water;
    lv_draw_rect_dsc_init(&rect_dsc_water);
    rect_dsc_water.bg_color = lv_color_hex(0x0000FF);
    rect_dsc_water.bg_opa = opacity_water;
    rect_dsc_water.border_color = lv_color_hex(0x000000);
    rect_dsc_water.border_width = 1;
    rect_dsc_water.radius = 1;
    lv_draw_rect(&layer, &rect_dsc_water, &coords);

    // Calculate center of mass pixel position
    int pixel_x = static_cast<int>((com.x + 1.0) * (WIDTH - 1) / 2.0);
    int pixel_y = static_cast<int>((com.y + 1.0) * (HEIGHT - 1) / 2.0);

    // Draw center of mass circle
    lv_draw_arc_dsc_t arc_dsc;
    lv_draw_arc_dsc_init(&arc_dsc);
    arc_dsc.color = lv_color_hex(0xFFFFFF);
    arc_dsc.center.x = pixel_x;
    arc_dsc.center.y = pixel_y;
    arc_dsc.width = 1;
    arc_dsc.radius = static_cast<int>(COM_VISUALIZATION_RADIUS);
    arc_dsc.start_angle = 0;
    arc_dsc.end_angle = 360;
    lv_draw_arc(&layer, &arc_dsc);

    // Draw velocity vector
    lv_draw_line_dsc_t velocity_line_dsc;
    lv_draw_line_dsc_init(&velocity_line_dsc);
    velocity_line_dsc.color = lv_color_hex(0x00FF00); // Green
    velocity_line_dsc.width = 2;
    velocity_line_dsc.opa = opacity_dirt;
    velocity_line_dsc.p1.x = pixel_x;
    velocity_line_dsc.p1.y = pixel_y;
    velocity_line_dsc.p2.x = pixel_x + static_cast<int>(v.x * VELOCITY_VISUALIZATION_SCALE);
    velocity_line_dsc.p2.y = pixel_y + static_cast<int>(v.y * VELOCITY_VISUALIZATION_SCALE);
    lv_draw_line(&layer, &velocity_line_dsc);

    // Draw pressure vector if significant
    lv_draw_line_dsc_t pressure_line_dsc;
    lv_draw_line_dsc_init(&pressure_line_dsc);
    pressure_line_dsc.color = lv_color_hex(0xFFFFFF); // White
    pressure_line_dsc.width = 2;
    pressure_line_dsc.opa = opacity_dirt;
    pressure_line_dsc.p1.x = WIDTH / 2;
    pressure_line_dsc.p1.y = HEIGHT / 2;
    pressure_line_dsc.p2.x =
        WIDTH / 2 + static_cast<int>(pressure.x * PRESSURE_VISUALIZATION_SCALE);
    pressure_line_dsc.p2.y =
        HEIGHT / 2 + static_cast<int>(pressure.y * PRESSURE_VISUALIZATION_SCALE);
    lv_draw_line(&layer, &pressure_line_dsc);

    lv_canvas_finish_layer(canvas, &layer);
}

void Cell::markDirty()
{
    needsRedraw = true;
}

std::string Cell::toString() const
{
    return "Cell{dirt=" + std::to_string(dirt) + ", water=" + std::to_string(water)
        + ", wood=" + std::to_string(wood) + ", leaf=" + std::to_string(leaf) + ", metal="
        + std::to_string(metal) + ", com=" + com.toString() + ", v=" + v.toString() + "}";
}

Vector2d Cell::getNormalizedDeflection() const
{
    // Normalize COM by the deflection threshold to get values in [-1, 1] range
    // This shows how deflected the COM is relative to the transfer threshold
    return Vector2d(com.x / COM_DEFLECTION_THRESHOLD, com.y / COM_DEFLECTION_THRESHOLD);
}

Vector2d Cell::calculateWaterCohesion(const Cell& cell, const Cell& neighbor) const
{
    // Only apply cohesion between water cells
    if (cell.water < World::MIN_DIRT_THRESHOLD || neighbor.water < World::MIN_DIRT_THRESHOLD) {
        return Vector2d(0.0, 0.0);
    }

    // Calculate force based on water amounts and distance
    double force = COHESION_STRENGTH * cell.water * neighbor.water;

    // Calculate direction vector between cells
    Vector2d direction = neighbor.com - cell.com;
    double distance = direction.mag();

    // Normalize and scale by force
    if (distance > 0.0) {
        return direction.normalize() * force;
    }
    return Vector2d(0.0, 0.0);
}

void Cell::applyViscosity(const Cell& neighbor)
{
    if (water < World::MIN_DIRT_THRESHOLD || neighbor.water < World::MIN_DIRT_THRESHOLD) {
        return;
    }

    // Average velocities based on water amounts
    double totalMass = water + neighbor.water;
    if (totalMass > 0) {
        Vector2d avgVelocity = (v * water + neighbor.v * neighbor.water) / totalMass;
        v = v + (avgVelocity - v) * VISCOSITY_FACTOR;
    }
}
