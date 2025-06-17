#include "Cell.h"

#include "World.h"

#include <algorithm>
#include <cassert>
#include <cctype>
#include <cmath>
#include <cstdio> // For snprintf
#include <cstring>
#include <iostream>
#include <string>

#include "lvgl/lvgl.h"

// Define ASSERT macro if not already defined
#ifndef ASSERT
#define ASSERT(cond, msg, ...) assert(cond)
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace {
// Drawing constants.
constexpr double COM_VISUALIZATION_RADIUS = 3.0;  // Larger for 100px cells
constexpr int VELOCITY_VISUALIZATION_SCALE = 5;   // Better visibility
constexpr int PRESSURE_VISUALIZATION_SCALE = 800; // Adjusted for larger cells
constexpr int DENSITY_GRID_SIZE = 10;             // Grid for density visualization
} // namespace

bool Cell::debugDraw = true;
uint32_t Cell::WIDTH = 100;  // Increased size for better detail visibility
uint32_t Cell::HEIGHT = 100; // Increased size for better detail visibility

// Initialize water physics constants
double Cell::COHESION_STRENGTH =
    0.5; // Default cohesion strength (increased for stronger water flow)
double Cell::VISCOSITY_FACTOR = 0.1;  // Default viscosity factor
double Cell::BUOYANCY_STRENGTH = 0.1; // Default buoyancy strength

Cell::Cell()
    : dirt(0.0),
      water(0.0),
      wood(0.0),
      leaf(0.0),
      metal(0.0),
      com(0.0, 0.0),
      v(0.0, 0.0),
      pressure(0.0, 0.0),
      buffer(WIDTH * HEIGHT * 4),
      canvas(nullptr),
      needsRedraw(true)
{}

Cell::~Cell()
{
    // Clean up the LVGL canvas object if it exists
    if (canvas != nullptr) {
        lv_obj_del(canvas);
        canvas = nullptr;
    }
}

// Copy constructor - don't copy LVGL objects for time reversal
Cell::Cell(const Cell& other)
    : dirt(other.dirt),
      water(other.water),
      wood(other.wood),
      leaf(other.leaf),
      metal(other.metal),
      com(other.com),
      v(other.v),
      pressure(other.pressure),
      buffer(other.buffer.size()) // Create new buffer with same size
      ,
      canvas(nullptr) // Don't copy LVGL object
      ,
      needsRedraw(true) // New copy needs redraw
{
    // Buffer contents will be regenerated when drawn
}

// Assignment operator - don't copy LVGL objects for time reversal
Cell& Cell::operator=(const Cell& other)
{
    if (this != &other) {
        dirt = other.dirt;
        water = other.water;
        wood = other.wood;
        leaf = other.leaf;
        metal = other.metal;
        com = other.com;
        v = other.v;
        pressure = other.pressure;

        // Resize buffer if needed but don't copy contents
        buffer.resize(other.buffer.size());

        // Don't copy LVGL object - keep our own canvas
        // canvas stays as is (either nullptr or our own object)
        needsRedraw = true; // Assignment means we need redraw
    }
    return *this;
}

void Cell::update(double newDirty, const Vector2d& newCom, const Vector2d& newV)
{
    // Check if the new dirt amount would cause overfill
    double currentTotal = water + wood + leaf + metal; // Total without dirt
    if (currentTotal + newDirty > 1.10) {
        // Clamp the dirt to fit within capacity, but don't create dirt
        dirt = std::max(0.0, std::min(newDirty, 1.10 - currentTotal));
    }
    else {
        dirt = newDirty;
    }

    com = newCom;
    v = newV;
    needsRedraw = true;
}

// Helper function to safely set a pixel on the canvas.
__attribute__((unused)) static void safe_set_pixel(lv_obj_t* canvas, int x, int y, lv_color_t color, lv_opa_t opa)
{
    if (x >= 0 && x < static_cast<int>(Cell::WIDTH) && y >= 0 && y < static_cast<int>(Cell::HEIGHT)) {
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

void Cell::drawNormal(lv_obj_t* /* parent */, uint32_t /* x */, uint32_t /* y */)
{
    lv_color_t brown = lv_color_hex(0x8B4513); // Saddle brown color

    // Calculate opacity based on dirt amount (0.0 to 1.0)
    lv_opa_t opacity_dirt = static_cast<lv_opa_t>(dirt * LV_OPA_COVER);
    lv_opa_t opacity_water = static_cast<lv_opa_t>(water * LV_OPA_COVER);

    lv_layer_t layer;
    lv_canvas_init_layer(canvas, &layer);

    // Draw black background for all cells
    lv_draw_rect_dsc_t bg_rect_dsc;
    lv_draw_rect_dsc_init(&bg_rect_dsc);
    bg_rect_dsc.bg_color = lv_color_hex(0x000000); // Black background
    bg_rect_dsc.bg_opa = LV_OPA_COVER;
    bg_rect_dsc.border_width = 0;
    lv_area_t coords = { 0, 0, static_cast<int32_t>(WIDTH), static_cast<int32_t>(HEIGHT) };
    lv_draw_rect(&layer, &bg_rect_dsc, &coords);

    // Draw dirt layer
    lv_draw_rect_dsc_t rect_dsc;
    lv_draw_rect_dsc_init(&rect_dsc);
    rect_dsc.bg_color = brown;
    rect_dsc.bg_opa = opacity_dirt;
    rect_dsc.border_color = brown; // Same color as background
    rect_dsc.border_opa =
        static_cast<lv_opa_t>(opacity_dirt * 0.3); // 30% of dirt opacity for softer look
    rect_dsc.border_width = 1;
    rect_dsc.radius = 1;
    lv_draw_rect(&layer, &rect_dsc, &coords);

    // Draw water layer on top
    lv_draw_rect_dsc_t rect_dsc_water;
    lv_draw_rect_dsc_init(&rect_dsc_water);
    rect_dsc_water.bg_color = lv_color_hex(0x0000FF);
    rect_dsc_water.bg_opa = opacity_water;
    rect_dsc_water.border_color = lv_color_hex(0x0000FF); // Same blue color as water
    rect_dsc_water.border_opa =
        static_cast<lv_opa_t>(opacity_water * 0.3); // 30% of water opacity for softer look
    rect_dsc_water.border_width = 1;
    rect_dsc_water.radius = 1;
    lv_draw_rect(&layer, &rect_dsc_water, &coords);

    lv_canvas_finish_layer(canvas, &layer);
}

void Cell::drawDebug(lv_obj_t* /* parent */, uint32_t /* x */, uint32_t /* y */)
{
    lv_color_t brown = lv_color_hex(0x8B4513); // Saddle brown color

    // Calculate opacity based on dirt amount (0.0 to 1.0)
    lv_opa_t opacity_dirt = static_cast<lv_opa_t>(dirt * LV_OPA_COVER);
    lv_opa_t opacity_water = static_cast<lv_opa_t>(water * LV_OPA_COVER);

    lv_layer_t layer;
    lv_canvas_init_layer(canvas, &layer);

    // Draw black background for all cells
    lv_draw_rect_dsc_t bg_rect_dsc;
    lv_draw_rect_dsc_init(&bg_rect_dsc);
    bg_rect_dsc.bg_color = lv_color_hex(0x000000); // Black background
    bg_rect_dsc.bg_opa = LV_OPA_COVER;
    bg_rect_dsc.border_width = 0;
    lv_area_t bg_coords = { 0, 0, static_cast<int32_t>(WIDTH), static_cast<int32_t>(HEIGHT) };
    lv_draw_rect(&layer, &bg_rect_dsc, &bg_coords);

    // // Draw density variation background using a gradient effect
    // double totalDensity = percentFull();
    // if (totalDensity > 0.01) {
    //     // Create density-based color variations
    //     for (int dy = 0; dy < DENSITY_GRID_SIZE; dy++) {
    //         for (int dx = 0; dx < DENSITY_GRID_SIZE; dx++) {
    //             // Simulate density variation within the cell
    //             double localDensity = totalDensity * (0.8 + 0.4 * sin(dx * 0.5) * cos(dy * 0.5));
    //             lv_opa_t localOpacity = static_cast<lv_opa_t>(localDensity * LV_OPA_COVER);

    //             lv_draw_rect_dsc_t rect_dsc;
    //             lv_draw_rect_dsc_init(&rect_dsc);
    //             rect_dsc.bg_color = (water > dirt) ? lv_color_hex(0x0066FF) : brown;
    //             rect_dsc.bg_opa = localOpacity;
    //             rect_dsc.border_width = 0;

    //             lv_area_t coords = {
    //                 dx * WIDTH / DENSITY_GRID_SIZE,
    //                 dy * HEIGHT / DENSITY_GRID_SIZE,
    //                 (dx + 1) * WIDTH / DENSITY_GRID_SIZE - 1,
    //                 (dy + 1) * HEIGHT / DENSITY_GRID_SIZE - 1
    //             };
    //             lv_draw_rect(&layer, &rect_dsc, &coords);
    //         }
    //     }
    // }

    // Draw dirt background with enhanced border
    lv_draw_rect_dsc_t rect_dsc;
    lv_draw_rect_dsc_init(&rect_dsc);
    rect_dsc.bg_color = brown;
    rect_dsc.bg_opa = static_cast<lv_opa_t>(opacity_dirt * 0.7); // More transparent for overlay
    rect_dsc.border_color = lv_color_hex(0x5D2A0A);              // Darker brown border
    rect_dsc.border_opa = opacity_dirt;
    rect_dsc.border_width = 2;
    rect_dsc.radius = 2;
    lv_area_t coords = { 0, 0, static_cast<int32_t>(WIDTH), static_cast<int32_t>(HEIGHT) };
    lv_draw_rect(&layer, &rect_dsc, &coords);

    // Draw water layer with enhanced visualization
    if (opacity_water > 0) {
        lv_draw_rect_dsc_t rect_dsc_water;
        lv_draw_rect_dsc_init(&rect_dsc_water);
        rect_dsc_water.bg_color = lv_color_hex(0x0066FF);
        rect_dsc_water.bg_opa = static_cast<lv_opa_t>(opacity_water * 0.8);
        rect_dsc_water.border_color = lv_color_hex(0x0044BB);
        rect_dsc_water.border_opa = opacity_water;
        rect_dsc_water.border_width = 2;
        rect_dsc_water.radius = 3;
        lv_draw_rect(&layer, &rect_dsc_water, &coords);
    }

    // Calculate center of mass pixel position
    int pixel_x = static_cast<int>((com.x + 1.0) * (WIDTH - 1) / 2.0);
    int pixel_y = static_cast<int>((com.y + 1.0) * (HEIGHT - 1) / 2.0);

    // Draw center of mass circle
    lv_draw_arc_dsc_t arc_dsc;
    lv_draw_arc_dsc_init(&arc_dsc);
    arc_dsc.color = lv_color_hex(0xFFFF00); // Bright yellow for better visibility
    arc_dsc.center.x = pixel_x;
    arc_dsc.center.y = pixel_y;
    arc_dsc.width = 1;
    arc_dsc.radius = static_cast<int>(COM_VISUALIZATION_RADIUS);
    arc_dsc.start_angle = 0;
    arc_dsc.end_angle = 360;
    lv_draw_arc(&layer, &arc_dsc);

    // Draw velocity vector with enhanced visualization
    if (v.mag() > 0.01) {
        lv_draw_line_dsc_t velocity_line_dsc;
        lv_draw_line_dsc_init(&velocity_line_dsc);
        velocity_line_dsc.color = lv_color_hex(0x00FF00); // Bright green
        velocity_line_dsc.width = 3;
        velocity_line_dsc.opa = LV_OPA_COVER;
        velocity_line_dsc.p1.x = pixel_x;
        velocity_line_dsc.p1.y = pixel_y;
        velocity_line_dsc.p2.x = pixel_x + static_cast<int>(v.x * VELOCITY_VISUALIZATION_SCALE);
        velocity_line_dsc.p2.y = pixel_y + static_cast<int>(v.y * VELOCITY_VISUALIZATION_SCALE);
        lv_draw_line(&layer, &velocity_line_dsc);

        // Add arrowhead for velocity direction
        int arrow_x = velocity_line_dsc.p2.x;
        int arrow_y = velocity_line_dsc.p2.y;
        double angle = atan2(v.y, v.x);
        int arrow_len = 8;

        lv_draw_line_dsc_t arrow_dsc = velocity_line_dsc;
        arrow_dsc.width = 2;

        // Left arrowhead line
        arrow_dsc.p1.x = arrow_x;
        arrow_dsc.p1.y = arrow_y;
        arrow_dsc.p2.x = arrow_x - arrow_len * cos(angle - M_PI / 6);
        arrow_dsc.p2.y = arrow_y - arrow_len * sin(angle - M_PI / 6);
        lv_draw_line(&layer, &arrow_dsc);

        // Right arrowhead line
        arrow_dsc.p2.x = arrow_x - arrow_len * cos(angle + M_PI / 6);
        arrow_dsc.p2.y = arrow_y - arrow_len * sin(angle + M_PI / 6);
        lv_draw_line(&layer, &arrow_dsc);
    }

    // Draw pressure vector with enhanced visualization
    if (pressure.mag() > 0.001) {
        lv_draw_line_dsc_t pressure_line_dsc;
        lv_draw_line_dsc_init(&pressure_line_dsc);
        pressure_line_dsc.color = lv_color_hex(0xFF0080); // Magenta for pressure
        pressure_line_dsc.width = 3;
        pressure_line_dsc.opa = LV_OPA_COVER;
        pressure_line_dsc.p1.x = WIDTH / 2;
        pressure_line_dsc.p1.y = HEIGHT / 2;
        pressure_line_dsc.p2.x =
            WIDTH / 2 + static_cast<int>(pressure.x * PRESSURE_VISUALIZATION_SCALE);
        pressure_line_dsc.p2.y =
            HEIGHT / 2 + static_cast<int>(pressure.y * PRESSURE_VISUALIZATION_SCALE);
        lv_draw_line(&layer, &pressure_line_dsc);
    }

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
    // Assert that COM components are finite and within reasonable bounds
    ASSERT(std::isfinite(com.x) && std::isfinite(com.y), "COM contains NaN or infinite values");
    ASSERT(std::abs(com.x) < 10.0 && std::abs(com.y) < 10.0, "COM values are unreasonably large");

    // Normalize COM by the deflection threshold to get values in [-1, 1] range
    // This shows how deflected the COM is relative to the transfer threshold
    return Vector2d(com.x / COM_DEFLECTION_THRESHOLD, com.y / COM_DEFLECTION_THRESHOLD);
}

double Cell::getEffectiveDensity() const
{
    double totalMass = dirt + water + wood + leaf + metal;

    // Return zero density for empty cells
    if (totalMass < World::MIN_MATTER_THRESHOLD) {
        return 0.0;
    }

    // Calculate weighted average density based on material composition
    double weightedDensity =
        (dirt * DIRT_DENSITY + water * WATER_DENSITY + wood * WOOD_DENSITY + leaf * LEAF_DENSITY
         + metal * METAL_DENSITY);

    return weightedDensity / totalMass;
}

void Cell::validateState(const std::string& /* context */) const
{
    ASSERT(std::isfinite(dirt), ("Cell dirt is NaN or infinite in " + context).c_str());
    ASSERT(std::isfinite(water), ("Cell water is NaN or infinite in " + context).c_str());
    ASSERT(
        std::isfinite(com.x) && std::isfinite(com.y),
        ("Cell COM is NaN or infinite in " + context).c_str());
    ASSERT(
        std::isfinite(v.x) && std::isfinite(v.y),
        ("Cell velocity is NaN or infinite in " + context).c_str());
    ASSERT(
        std::isfinite(percentFull()),
        ("Cell percentFull is NaN or infinite in " + context).c_str());
    ASSERT(dirt >= 0.0, ("Cell dirt is negative in " + context).c_str());
    ASSERT(water >= 0.0, ("Cell water is negative in " + context).c_str());
    ASSERT(
        percentFull() <= 1.10, // Increased tolerance while we tune the density mechanics
        ("Cell overfull in " + context).c_str());
}

Vector2d Cell::calculateWaterCohesion(
    const Cell& cell,
    const Cell& neighbor,
    const World* world,
    uint32_t cellX,
    uint32_t cellY) const
{
    // Only apply cohesion between water cells
    if (cell.water < World::MIN_MATTER_THRESHOLD || neighbor.water < World::MIN_MATTER_THRESHOLD) {
        return Vector2d(0.0, 0.0);
    }

    // Calculate local water mass in a 2-cell radius for mass-weighted attraction
    double localWaterMass = 0.0;
    const int MASS_RADIUS = 2;

    if (world != nullptr) {
        for (int dy = -MASS_RADIUS; dy <= MASS_RADIUS; dy++) {
            for (int dx = -MASS_RADIUS; dx <= MASS_RADIUS; dx++) {
                int nx = static_cast<int>(cellX) + dx;
                int ny = static_cast<int>(cellY) + dy;

                // Check bounds
                if (nx >= 0 && nx < static_cast<int>(world->getWidth()) && ny >= 0
                    && ny < static_cast<int>(world->getHeight())) {
                    localWaterMass += world->at(nx, ny).water;
                }
            }
        }
    }

    // Mass attraction bonus: logarithmic scaling to prevent excessive forces
    // More water nearby = stronger attraction, but with diminishing returns
    static constexpr double MASS_ATTRACTION_FACTOR = 0.5;
    double massAttractionBonus = std::log(1.0 + localWaterMass) * MASS_ATTRACTION_FACTOR;

    // Enhanced cohesion strength with mass weighting
    double enhancedCohesionStrength = COHESION_STRENGTH + massAttractionBonus;

    // Calculate force based on water amounts and enhanced cohesion
    double force = enhancedCohesionStrength * cell.water * neighbor.water;

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
    if (water < World::MIN_MATTER_THRESHOLD || neighbor.water < World::MIN_MATTER_THRESHOLD) {
        return;
    }

    // Average velocities based on water amounts
    double totalMass = water + neighbor.water;
    if (totalMass > 0) {
        Vector2d avgVelocity = (v * water + neighbor.v * neighbor.water) / totalMass;
        v = v + (avgVelocity - v) * VISCOSITY_FACTOR;
    }
}

Vector2d Cell::calculateBuoyancy(const Cell& cell, const Cell& neighbor, const Vector2i& offset) const
{
    // Get effective densities of both cells
    double cellDensity = cell.getEffectiveDensity();
    double neighborDensity = neighbor.getEffectiveDensity();

    // Skip if either cell is effectively empty
    if (cellDensity <= 0.0 || neighborDensity <= 0.0) {
        return Vector2d(0.0, 0.0);
    }

    // Buoyancy only occurs when denser material is surrounded by less dense material
    // If this cell is not denser than the neighbor, no buoyancy force
    if (cellDensity <= neighborDensity) {
        return Vector2d(0.0, 0.0);
    }

    // Calculate density difference - foundation of Archimedes' principle
    double densityDiff = cellDensity - neighborDensity;

    // Buoyant force proportional to density difference and displaced volume
    // The cell.percentFull() represents the volume of material experiencing buoyancy
    double buoyantForce = BUOYANCY_STRENGTH * cell.percentFull() * densityDiff;

    Vector2d buoyancyForce = Vector2d(0.0, 0.0);

    // Apply upward buoyancy if neighbor is below (offset.y > 0)
    // This simulates denser material being pushed up by less dense material below
    if (offset.y > 0) {
        buoyancyForce.y = -buoyantForce; // Upward force (negative y)
    }

    // Apply lateral buoyancy for horizontal displacement
    // Weaker effect for side-to-side movement, but still helps with separation
    if (offset.x != 0) {
        double lateralForce = buoyantForce * 0.3; // Reduced lateral effect
        buoyancyForce.x = -offset.x * lateralForce;     // Push away from denser neighbor
    }

    return buoyancyForce;
}

// =================================================================
// CELLINTERFACE IMPLEMENTATION
// =================================================================

void Cell::addDirt(double amount)
{
    if (amount <= 0.0) return;
    safeAddMaterial(dirt, amount);
    markDirty();
}

void Cell::addWater(double amount)
{
    if (amount <= 0.0) return;
    safeAddMaterial(water, amount);
    markDirty();
}

void Cell::addDirtWithVelocity(double amount, const Vector2d& velocity)
{
    if (amount <= 0.0) return;
    safeAddMaterial(dirt, amount);
    
    // Update velocity based on momentum conservation
    double totalMaterial = getTotalMaterial();
    if (totalMaterial > 0.0) {
        // Weighted average of existing velocity and new velocity
        v = (v * (totalMaterial - amount) + velocity * amount) / totalMaterial;
    } else {
        v = velocity;
    }
    markDirty();
}

void Cell::addWaterWithVelocity(double amount, const Vector2d& velocity)
{
    if (amount <= 0.0) return;
    safeAddMaterial(water, amount);
    
    // Update velocity based on momentum conservation
    double totalMaterial = getTotalMaterial();
    if (totalMaterial > 0.0) {
        // Weighted average of existing velocity and new velocity
        v = (v * (totalMaterial - amount) + velocity * amount) / totalMaterial;
    } else {
        v = velocity;
    }
    markDirty();
}

void Cell::addDirtWithCOM(double amount, const Vector2d& comOffset, const Vector2d& velocity)
{
    if (amount <= 0.0) return;
    safeAddMaterial(dirt, amount);
    
    // Update center of mass based on weighted average
    double totalMaterial = getTotalMaterial();
    if (totalMaterial > 0.0) {
        // Weighted average of existing COM and new COM offset
        com = (com * (totalMaterial - amount) + comOffset * amount) / totalMaterial;
        // Clamp COM to valid bounds [-1, 1]
        com.x = std::max(-1.0, std::min(1.0, com.x));
        com.y = std::max(-1.0, std::min(1.0, com.y));
    } else {
        com = comOffset;
    }
    
    // Update velocity as well
    if (totalMaterial > 0.0) {
        v = (v * (totalMaterial - amount) + velocity * amount) / totalMaterial;
    } else {
        v = velocity;
    }
    markDirty();
}

void Cell::clear()
{
    dirt = 0.0;
    water = 0.0;
    wood = 0.0;
    leaf = 0.0;
    metal = 0.0;
    com = Vector2d(0.0, 0.0);
    v = Vector2d(0.0, 0.0);
    pressure = Vector2d(0.0, 0.0);
    markDirty();
}

double Cell::getTotalMaterial() const
{
    return percentFull();
}

bool Cell::isEmpty() const
{
    return getTotalMaterial() < 0.001; // Use small threshold for "empty"
}

std::string Cell::toAsciiCharacter() const
{
    if (isEmpty()) {
        return "  ";  // Two spaces for empty cells (2x1 format)
    }
    
    // Find the dominant material in this cell
    double max_amount = 0.0;
    char material_char = ' ';
    
    if (dirt > max_amount) {
        max_amount = dirt;
        material_char = '#';  // Dirt
    }
    if (water > max_amount) {
        max_amount = water;
        material_char = '~';  // Water
    }
    if (wood > max_amount) {
        max_amount = wood;
        material_char = 'W';  // Wood
    }
    if (leaf > max_amount) {
        max_amount = leaf;
        material_char = 'L';  // Leaf
    }
    if (metal > max_amount) {
        max_amount = metal;
        material_char = 'M';  // Metal
    }
    
    // Convert total material to 0-9 scale
    double total_material = getTotalMaterial();
    int fill_level = static_cast<int>(std::round(total_material * 9.0));
    fill_level = std::clamp(fill_level, 0, 9);
    
    // Return 2-character representation: material + fill level
    return std::string(1, material_char) + std::to_string(fill_level);
}
