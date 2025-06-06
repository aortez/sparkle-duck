#include "Cell.h"

#include "World.h"

#include <cstdio> // For snprintf
#include <cstring>
#include <string>

#include "lvgl/lvgl.h"

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

Cell::~Cell()
{
    // No explicit cleanup needed for vector
}

void Cell::update(double newDirty, const Vector2d& newCom, const Vector2d& newV)
{
    dirt = newDirty; // For backward compatibility, we'll use dirt as the main element for now
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

    lv_color_t black = lv_color_hex(0x000000);
    lv_color_t brown = lv_color_hex(0x8B4513); // Saddle brown color
    lv_color_t yellow = lv_color_hex(0xFFFF00);
    lv_color_t white = lv_color_hex(0xFFFFFF);

    // Zero buffer.
    std::fill(buffer.begin(), buffer.end(), 0);

    // Calculate opacity based on dirt amount (0.0 to 1.0)
    lv_opa_t opacity_dirt = static_cast<lv_opa_t>(dirt * LV_OPA_COVER);
    lv_opa_t opacity_water = static_cast<lv_opa_t>(water * LV_OPA_COVER);

    if (!debugDraw) {
        // Normal mode: draw dirt and water layers
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
    else {
        lv_layer_t layer;
        lv_canvas_init_layer(canvas, &layer);

        // Debug mode:
        // 1. Fill background brown (opaque) and draw black border:
        lv_draw_rect_dsc_t rect_dsc;
        lv_draw_rect_dsc_init(&rect_dsc);
        rect_dsc.bg_color = brown;
        rect_dsc.bg_opa = opacity_dirt;
        rect_dsc.border_color = lv_color_hex(0x000000);
        rect_dsc.border_width = 1;
        rect_dsc.radius = 1;
        lv_area_t coords = { 0, 0, WIDTH, HEIGHT };
        lv_draw_rect(&layer, &rect_dsc, &coords);

        // 1.5. Draw transparent blue water.
        lv_draw_rect_dsc_t rect_dsc_water;
        lv_draw_rect_dsc_init(&rect_dsc_water);
        rect_dsc_water.bg_color = lv_color_hex(0x0000FF);
        rect_dsc_water.bg_opa = opacity_water;
        rect_dsc_water.border_color = lv_color_hex(0x000000);
        rect_dsc_water.border_width = 1;
        rect_dsc_water.radius = 1;
        lv_draw_rect(&layer, &rect_dsc_water, &coords);

        // 2. Draw center of mass.
        int pixel_x = static_cast<int>((com.x + 1.0) * (WIDTH - 1) / 2.0);
        int pixel_y = static_cast<int>((com.y + 1.0) * (HEIGHT - 1) / 2.0);

        // 3. Draw center of mass.
        lv_draw_arc_dsc_t dsc;
        lv_draw_arc_dsc_init(&dsc);
        dsc.color = lv_color_hex(0xFFFFFF);
        dsc.center.x = pixel_x;
        dsc.center.y = pixel_y;
        dsc.width = 1;
        dsc.radius = 3;
        dsc.start_angle = 0;
        dsc.end_angle = 360;

        lv_draw_arc(&layer, &dsc);

        // 4. Draw green velocity vector.
        lv_draw_line_dsc_t line_dsc;
        lv_draw_line_dsc_init(&line_dsc);
        line_dsc.color = lv_color_hex(0x00FF00);
        line_dsc.width = 2;
        line_dsc.opa = opacity_dirt;
        // Originate from cell center.
        line_dsc.p1.x = pixel_x;
        line_dsc.p1.y = pixel_y;
        line_dsc.p2.x = pixel_x + static_cast<int>(v.x);
        line_dsc.p2.y = pixel_y + static_cast<int>(v.y);

        lv_draw_line(&layer, &line_dsc);

        // Draw red pressure vector if there is pressure
        if (pressure.mag() > 0.01) { // Only draw if pressure is significant
            lv_draw_line_dsc_t pressure_dsc;
            lv_draw_line_dsc_init(&pressure_dsc);
            pressure_dsc.color = lv_color_hex(0xFFFFFF); // White color
            pressure_dsc.width = 2;
            pressure_dsc.opa = opacity_dirt;
            // Originate from cell center.
            pressure_dsc.p1.x = WIDTH / 2;
            pressure_dsc.p1.y = HEIGHT / 2;
            pressure_dsc.p2.x =
                WIDTH / 2 + static_cast<int>(pressure.x * 20.0); // Scale up for visibility
            pressure_dsc.p2.y = HEIGHT / 2 + static_cast<int>(pressure.y * 20.0);

            lv_draw_line(&layer, &pressure_dsc);
        }

        lv_canvas_finish_layer(canvas, &layer);
    }

    // Mark that we've drawn the cell
    needsRedraw = false;
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

Vector2d Cell::calculateWaterCohesion(const Cell& cell, const Cell& neighbor) const
{
    // Only apply cohesion between water cells
    if (cell.water < World::MIN_DIRT_THRESHOLD || neighbor.water < World::MIN_DIRT_THRESHOLD) {
        return Vector2d(0.0, 0.0);
    }

    // Calculate force based on water amounts and distance
    const double COHESION_STRENGTH = 0.1;
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
    const double VISCOSITY_FACTOR = 0.1;
    double totalMass = water + neighbor.water;
    if (totalMass > 0) {
        Vector2d avgVelocity = (v * water + neighbor.v * neighbor.water) / totalMass;
        v = v + (avgVelocity - v) * VISCOSITY_FACTOR;
    }
}
