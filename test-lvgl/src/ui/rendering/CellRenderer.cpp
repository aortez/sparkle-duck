#include "CellRenderer.h"
#include "core/MaterialType.h"
#include <algorithm>
#include <spdlog/spdlog.h>

namespace DirtSim {
namespace Ui {

// Material color mapping (from CellB.cpp on main branch).
static lv_color_t getMaterialColor(MaterialType type)
{
    switch (type) {
        case MaterialType::AIR:
            return lv_color_hex(0x000000); // Black.
        case MaterialType::DIRT:
            return lv_color_hex(0xA0522D); // Sienna brown.
        case MaterialType::LEAF:
            return lv_color_hex(0x00FF32); // Bright lime green.
        case MaterialType::METAL:
            return lv_color_hex(0xC0C0C0); // Silver.
        case MaterialType::SAND:
            return lv_color_hex(0xFFB347); // Sandy orange.
        case MaterialType::SEED:
            return lv_color_hex(0xFFD700); // Gold (bright and distinctive).
        case MaterialType::WALL:
            return lv_color_hex(0x808080); // Gray.
        case MaterialType::WATER:
            return lv_color_hex(0x00BFFF); // Deep sky blue.
        case MaterialType::WOOD:
            return lv_color_hex(0xDEB887); // Burlywood.
        default:
            return lv_color_hex(0xFF00FF); // Magenta for unknown.
    }
}

CellRenderer::~CellRenderer()
{
    cleanup();
}

void CellRenderer::initialize(lv_obj_t* parent, uint32_t worldWidth, uint32_t worldHeight)
{
    spdlog::info("CellRenderer: Initializing for {}x{} world", worldWidth, worldHeight);

    parent_ = parent;
    width_ = worldWidth;
    height_ = worldHeight;

    // Offset grid to the right of control panel (260px wide).
    constexpr int32_t CONTROL_PANEL_WIDTH = 260;

    // Create canvas grid for each cell.
    canvases_.resize(worldHeight);
    for (uint32_t y = 0; y < worldHeight; ++y) {
        canvases_[y].resize(worldWidth);
        for (uint32_t x = 0; x < worldWidth; ++x) {
            auto& cellCanvas = canvases_[y][x];

            // Create canvas object.
            cellCanvas.canvas = lv_canvas_create(parent);

            // Allocate buffer for 30x30 pixels (ARGB8888 = 4 bytes per pixel).
            constexpr size_t bufferSize = Cell::WIDTH * Cell::HEIGHT * 4;
            cellCanvas.buffer.resize(bufferSize);

            // Set canvas buffer.
            lv_canvas_set_buffer(
                cellCanvas.canvas,
                cellCanvas.buffer.data(),
                Cell::WIDTH,
                Cell::HEIGHT,
                LV_COLOR_FORMAT_ARGB8888);

            // Position canvas to the right of control panel.
            lv_obj_set_pos(
                cellCanvas.canvas, CONTROL_PANEL_WIDTH + x * Cell::WIDTH, y * Cell::HEIGHT);
        }
    }

    spdlog::info("CellRenderer: Initialized {} cell canvases", worldWidth * worldHeight);
}

void CellRenderer::resize(lv_obj_t* parent, uint32_t worldWidth, uint32_t worldHeight)
{
    spdlog::info(
        "CellRenderer: Resizing from {}x{} to {}x{}", width_, height_, worldWidth, worldHeight);

    cleanup();
    initialize(parent, worldWidth, worldHeight);
}

void CellRenderer::renderWorldData(const WorldData& worldData, lv_obj_t* parent, bool debugDraw)
{
    // Initialize/resize if needed.
    if (!parent_ || parent_ != parent || width_ != worldData.width || height_ != worldData.height) {
        if (width_ != worldData.width || height_ != worldData.height) {
            resize(parent, worldData.width, worldData.height);
        }
        else {
            initialize(parent, worldData.width, worldData.height);
        }
    }

    // Render each cell.
    for (uint32_t y = 0; y < worldData.height; ++y) {
        for (uint32_t x = 0; x < worldData.width; ++x) {
            const Cell& cell = worldData.cells[y * worldData.width + x];
            renderCell(const_cast<Cell&>(cell), x, y, debugDraw);
        }
    }
}

void CellRenderer::cleanup()
{
    spdlog::debug("CellRenderer: Cleaning up {} canvases", canvases_.size());

    for (auto& row : canvases_) {
        for (auto& cellCanvas : row) {
            if (cellCanvas.canvas) {
                lv_obj_del(cellCanvas.canvas);
                cellCanvas.canvas = nullptr;
            }
            cellCanvas.buffer.clear();
        }
        row.clear();
    }
    canvases_.clear();

    width_ = 0;
    height_ = 0;
    parent_ = nullptr;
}

void CellRenderer::renderCell(Cell& cell, uint32_t x, uint32_t y, bool debugDraw)
{
    if (y >= canvases_.size() || x >= canvases_[y].size()) {
        return; // Out of bounds.
    }

    auto& cellCanvas = canvases_[y][x];
    if (!cellCanvas.canvas) {
        return; // Canvas not initialized.
    }

    if (debugDraw) {
        renderCellDebug(cell, cellCanvas, x, y);
    }
    else {
        renderCellNormal(cell, cellCanvas, x, y);
    }
}

void CellRenderer::renderCellNormal(Cell& cell, CellCanvas& canvas, uint32_t /*x*/, uint32_t /*y*/)
{
    // Clear buffer and initialize layer (from CellB::drawNormal).
    std::fill(canvas.buffer.begin(), canvas.buffer.end(), 0);

    lv_layer_t layer;
    lv_canvas_init_layer(canvas.canvas, &layer);

    // Black background for all cells.
    lv_draw_rect_dsc_t bg_rect_dsc;
    lv_draw_rect_dsc_init(&bg_rect_dsc);
    bg_rect_dsc.bg_color = lv_color_hex(0x000000);
    bg_rect_dsc.bg_opa = LV_OPA_COVER;
    bg_rect_dsc.border_width = 0;
    lv_area_t bg_coords = {
        0, 0, static_cast<int32_t>(Cell::WIDTH - 1), static_cast<int32_t>(Cell::HEIGHT - 1)
    };
    lv_draw_rect(&layer, &bg_rect_dsc, &bg_coords);

    // Render material if not empty.
    if (!cell.isEmpty() && cell.material_type != MaterialType::AIR) {
        lv_color_t material_color = getMaterialColor(cell.material_type);
        lv_opa_t opacity =
            static_cast<lv_opa_t>(cell.fill_ratio * static_cast<double>(LV_OPA_COVER));

        lv_draw_rect_dsc_t rect_dsc;
        lv_draw_rect_dsc_init(&rect_dsc);
        rect_dsc.bg_color = material_color;
        rect_dsc.bg_opa = static_cast<lv_opa_t>(opacity * 0.7);
        rect_dsc.border_color = material_color;
        rect_dsc.border_opa = opacity;
        rect_dsc.border_width = 2;
        rect_dsc.radius = 2;

        lv_area_t coords = {
            0, 0, static_cast<int32_t>(Cell::WIDTH - 1), static_cast<int32_t>(Cell::HEIGHT - 1)
        };
        lv_draw_rect(&layer, &rect_dsc, &coords);
    }

    lv_canvas_finish_layer(canvas.canvas, &layer);
}

void CellRenderer::renderCellDebug(Cell& cell, CellCanvas& canvas, uint32_t /*x*/, uint32_t /*y*/)
{
    // Clear buffer and initialize layer (from CellB::drawDebug).
    std::fill(canvas.buffer.begin(), canvas.buffer.end(), 0);

    lv_layer_t layer;
    lv_canvas_init_layer(canvas.canvas, &layer);

    // Black background.
    lv_draw_rect_dsc_t bg_rect_dsc;
    lv_draw_rect_dsc_init(&bg_rect_dsc);
    bg_rect_dsc.bg_color = lv_color_hex(0x000000);
    bg_rect_dsc.bg_opa = LV_OPA_COVER;
    bg_rect_dsc.border_width = 0;
    lv_area_t coords = {
        0, 0, static_cast<int32_t>(Cell::WIDTH - 1), static_cast<int32_t>(Cell::HEIGHT - 1)
    };
    lv_draw_rect(&layer, &bg_rect_dsc, &coords);

    if (!cell.isEmpty() && cell.material_type != MaterialType::AIR) {
        lv_color_t material_color = getMaterialColor(cell.material_type);
        lv_opa_t opacity =
            static_cast<lv_opa_t>(cell.fill_ratio * static_cast<double>(LV_OPA_COVER));

        // Base material rendering.
        lv_draw_rect_dsc_t rect_dsc;
        lv_draw_rect_dsc_init(&rect_dsc);
        rect_dsc.bg_color = material_color;
        rect_dsc.bg_opa = static_cast<lv_opa_t>(opacity * 0.7);
        rect_dsc.border_color = material_color;
        rect_dsc.border_opa = opacity;
        rect_dsc.border_width = 2;
        rect_dsc.radius = 2;
        lv_draw_rect(&layer, &rect_dsc, &coords);

        // Center of mass indicator (yellow square).
        int com_pixel_x = static_cast<int>((cell.com.x + 1.0) * (Cell::WIDTH - 1) / 2.0);
        int com_pixel_y = static_cast<int>((cell.com.y + 1.0) * (Cell::HEIGHT - 1) / 2.0);

        const int square_size = 6;
        const int half_size = square_size / 2;

        lv_draw_rect_dsc_t com_rect_dsc;
        lv_draw_rect_dsc_init(&com_rect_dsc);
        com_rect_dsc.bg_color = lv_color_hex(0xFFFF00); // Bright yellow.
        com_rect_dsc.bg_opa = LV_OPA_COVER;
        com_rect_dsc.border_color = lv_color_hex(0xCC9900); // Darker yellow border.
        com_rect_dsc.border_opa = LV_OPA_COVER;
        com_rect_dsc.border_width = 1;
        com_rect_dsc.radius = 0;

        lv_area_t com_coords = { com_pixel_x - half_size,
                                 com_pixel_y - half_size,
                                 com_pixel_x + half_size - 1,
                                 com_pixel_y + half_size - 1 };
        lv_draw_rect(&layer, &com_rect_dsc, &com_coords);

        // Velocity vector (green line).
        if (cell.velocity.magnitude() > 0.01) {
            double scale = 20.0;
            int end_x = com_pixel_x + static_cast<int>(cell.velocity.x * scale);
            int end_y = com_pixel_y + static_cast<int>(cell.velocity.y * scale);

            lv_draw_line_dsc_t line_dsc;
            lv_draw_line_dsc_init(&line_dsc);
            line_dsc.color = lv_color_hex(0x00FF00); // Bright green.
            line_dsc.width = 2;
            line_dsc.p1.x = com_pixel_x;
            line_dsc.p1.y = com_pixel_y;
            line_dsc.p2.x = end_x;
            line_dsc.p2.y = end_y;
            lv_draw_line(&layer, &line_dsc);
        }
    }

    lv_canvas_finish_layer(canvas.canvas, &layer);
}

} // namespace Ui
} // namespace DirtSim
