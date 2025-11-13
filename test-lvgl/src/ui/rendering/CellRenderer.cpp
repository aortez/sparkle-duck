#include "CellRenderer.h"
#include "core/MaterialType.h"
#include <algorithm>
#include <cassert>
#include <new> // for std::bad_alloc
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

void CellRenderer::calculateScaling(uint32_t worldWidth, uint32_t worldHeight)
{
    // With fixed-size canvas, calculate how many pixels each cell gets
    if (canvasWidth_ == 0 || canvasHeight_ == 0) {
        spdlog::debug("CellRenderer: Canvas not yet created, deferring scaling calculation");
        return;
    }

    if (worldWidth == 0 || worldHeight == 0) {
        spdlog::warn(
            "CellRenderer: Invalid world dimensions for scaling ({}x{})", worldWidth, worldHeight);
        return;
    }

    // Calculate pixels per cell to fit world in fixed canvas
    double pixelsPerCellX = (double)canvasWidth_ / worldWidth;
    double pixelsPerCellY = (double)canvasHeight_ / worldHeight;

    // Use the smaller value to preserve aspect ratio
    double pixelsPerCell = std::min(pixelsPerCellX, pixelsPerCellY);

    // Ensure minimum cell size of 2x2 pixels
    pixelsPerCell = std::max(2.0, pixelsPerCell);

    scaledCellWidth_ = static_cast<uint32_t>(pixelsPerCell);
    scaledCellHeight_ = static_cast<uint32_t>(pixelsPerCell);

    // Calculate the scale factor relative to base Cell::WIDTH
    scaleX_ = pixelsPerCell / Cell::WIDTH;
    scaleY_ = pixelsPerCell / Cell::HEIGHT;

    spdlog::debug(
        "CellRenderer: Calculated scaling for {}x{} world - {} pixels per cell (scale {:.2f})",
        worldWidth,
        worldHeight,
        scaledCellWidth_,
        scaleX_);
}

void CellRenderer::initialize(lv_obj_t* parent, uint32_t worldWidth, uint32_t worldHeight)
{
    spdlog::info("CellRenderer: Initializing fixed-size canvas for UI container");

    // Validate input parameters
    if (!parent) {
        spdlog::error("CellRenderer: Invalid parent for initialization");
        return;
    }

    // Only initialize once - canvas stays fixed size
    if (worldCanvas_) {
        spdlog::debug("CellRenderer: Canvas already initialized, skipping");
        return;
    }

    parent_ = parent;
    width_ = worldWidth;
    height_ = worldHeight;

    // Get container size - this is our canvas size (fixed)
    int32_t containerWidth = lv_obj_get_width(parent);
    int32_t containerHeight = lv_obj_get_height(parent);

    // Sanity check container dimensions
    if (containerWidth <= 0 || containerHeight <= 0) {
        spdlog::warn(
            "CellRenderer: Invalid container dimensions {}x{}, using defaults",
            containerWidth,
            containerHeight);
        containerWidth = 800;
        containerHeight = 600;
    }

    // Use 90% of container size for the canvas
    canvasWidth_ = static_cast<uint32_t>(containerWidth * 0.9);
    canvasHeight_ = static_cast<uint32_t>(containerHeight * 0.9);

    // Center the canvas in the container
    int32_t offsetX = (containerWidth - canvasWidth_) / 2;
    int32_t offsetY = (containerHeight - canvasHeight_) / 2;

    // Ensure offsets are non-negative
    if (offsetX < 0) offsetX = 0;
    if (offsetY < 0) offsetY = 0;

    // Create single fixed-size canvas
    worldCanvas_ = lv_canvas_create(parent);
    if (!worldCanvas_) {
        spdlog::error("CellRenderer: Failed to create canvas");
        return;
    }

    // Allocate buffer for canvas at container resolution (fixed size)
    size_t bufferSize = static_cast<size_t>(canvasWidth_) * canvasHeight_ * 4; // ARGB8888

    try {
        canvasBuffer_.resize(bufferSize);
        std::fill(canvasBuffer_.begin(), canvasBuffer_.end(), 0);
    }
    catch (const std::bad_alloc& e) {
        spdlog::error(
            "CellRenderer: Failed to allocate buffer of size {} bytes: {}", bufferSize, e.what());
        lv_obj_del(worldCanvas_);
        worldCanvas_ = nullptr;
        return;
    }

    // Verify buffer was allocated successfully
    if (canvasBuffer_.empty() || canvasBuffer_.data() == nullptr) {
        spdlog::error("CellRenderer: Buffer allocation succeeded but data pointer is null");
        lv_obj_del(worldCanvas_);
        worldCanvas_ = nullptr;
        return;
    }

    // Set canvas buffer (this never changes)
    lv_canvas_set_buffer(
        worldCanvas_, canvasBuffer_.data(), canvasWidth_, canvasHeight_, LV_COLOR_FORMAT_ARGB8888);

    // Position canvas
    lv_obj_set_pos(worldCanvas_, offsetX, offsetY);

    // Now calculate scaling for the world size
    calculateScaling(worldWidth, worldHeight);

    spdlog::info(
        "CellRenderer: Initialized fixed canvas {}x{} pixels (will scale world dynamically)",
        canvasWidth_,
        canvasHeight_);
}

void CellRenderer::resize(lv_obj_t* parent, uint32_t worldWidth, uint32_t worldHeight)
{
    spdlog::info(
        "CellRenderer: Updating world size from {}x{} to {}x{}",
        width_,
        height_,
        worldWidth,
        worldHeight);

    // Only update if dimensions actually changed
    if (width_ == worldWidth && height_ == worldHeight && parent_ == parent) {
        spdlog::debug("CellRenderer: No size change, skipping");
        return;
    }

    // Update world dimensions
    width_ = worldWidth;
    height_ = worldHeight;
    parent_ = parent;

    // Recalculate scaling for new world size (canvas size stays the same)
    calculateScaling(worldWidth, worldHeight);

    spdlog::info(
        "CellRenderer: Updated to {}x{} cells at {}x{} pixels each",
        worldWidth,
        worldHeight,
        scaledCellWidth_,
        scaledCellHeight_);
}

void CellRenderer::renderWorldData(
    const WorldData& worldData, lv_obj_t* parent, bool debugDraw, bool usePixelRenderer)
{
    // Validate input
    if (!parent || worldData.width == 0 || worldData.height == 0) {
        spdlog::warn(
            "CellRenderer: Invalid render parameters (parent={}, size={}x{})",
            (void*)parent,
            worldData.width,
            worldData.height);
        return;
    }

    // Initialize canvas on first call
    if (!worldCanvas_) {
        initialize(parent, worldData.width, worldData.height);
        if (!worldCanvas_) {
            return; // Failed to initialize
        }
    }

    // Update scaling if world dimensions changed
    if (width_ != worldData.width || height_ != worldData.height) {
        resize(parent, worldData.width, worldData.height);
    }

    // Check if canvas is still valid
    if (!lv_obj_is_valid(worldCanvas_)) {
        spdlog::error("CellRenderer: Canvas is no longer valid, needs reinitialization");
        worldCanvas_ = nullptr;
        return;
    }

    // Clear buffer
    std::fill(canvasBuffer_.begin(), canvasBuffer_.end(), 0);

    // Log the first render after a resize for debugging
    static uint32_t lastWidth = 0;
    static uint32_t lastHeight = 0;
    if (lastWidth != worldData.width || lastHeight != worldData.height) {
        spdlog::info(
            "CellRenderer: First render at {}x{} world, {}x{} canvas, {}x{} cell size",
            worldData.width,
            worldData.height,
            canvasWidth_,
            canvasHeight_,
            scaledCellWidth_,
            scaledCellHeight_);
        int32_t totalW = worldData.width * scaledCellWidth_;
        int32_t totalH = worldData.height * scaledCellHeight_;
        spdlog::info(
            "CellRenderer: Total world rendering size {}x{}, fits in canvas? W:{} H:{}",
            totalW,
            totalH,
            totalW <= canvasWidth_,
            totalH <= canvasHeight_);
        lastWidth = worldData.width;
        lastHeight = worldData.height;
    }

    // Calculate world centering offset once for entire frame
    int32_t totalWorldWidth = worldData.width * scaledCellWidth_;
    int32_t totalWorldHeight = worldData.height * scaledCellHeight_;
    int32_t renderOffsetX = (canvasWidth_ - totalWorldWidth) / 2;
    int32_t renderOffsetY = (canvasHeight_ - totalWorldHeight) / 2;
    if (renderOffsetX < 0) renderOffsetX = 0;
    if (renderOffsetY < 0) renderOffsetY = 0;

    if (usePixelRenderer) {
        // FAST PATH: Direct pixel rendering (no LVGL layer)
        uint32_t* pixels = reinterpret_cast<uint32_t*>(canvasBuffer_.data());

        for (uint32_t y = 0; y < worldData.height; ++y) {
            for (uint32_t x = 0; x < worldData.width; ++x) {
                uint32_t idx = y * worldData.width + x;
                if (idx >= worldData.cells.size()) break;

                const Cell& cell = worldData.cells[idx];
                int32_t cellX = renderOffsetX + x * scaledCellWidth_;
                int32_t cellY = renderOffsetY + y * scaledCellHeight_;

                // Bounds check
                if (cellX < 0 || cellY < 0 || cellX + scaledCellWidth_ > canvasWidth_
                    || cellY + scaledCellHeight_ > canvasHeight_) {
                    continue;
                }

                // Determine cell color
                uint32_t cellColor = 0xFF000000; // ARGB black

                if (!cell.isEmpty() && cell.material_type != MaterialType::AIR) {
                    lv_color_t matColor = getMaterialColor(cell.material_type);
                    uint8_t alpha = static_cast<uint8_t>(cell.fill_ratio * 255.0 * 0.7);

                    // Convert to ARGB32
                    cellColor = (alpha << 24) | (matColor.red << 16) | (matColor.green << 8)
                        | matColor.blue;
                }

                // Fill cell rectangle
                for (uint32_t py = 0; py < scaledCellHeight_; py++) {
                    uint32_t rowStart = (cellY + py) * canvasWidth_ + cellX;
                    for (uint32_t px = 0; px < scaledCellWidth_; px++) {
                        pixels[rowStart + px] = cellColor;
                    }
                }
            }
        }

        // Invalidate canvas to trigger display update
        lv_obj_invalidate(worldCanvas_);
    }
    else {
        // SLOW PATH: LVGL layer rendering
        lv_layer_t layer;
        lv_canvas_init_layer(worldCanvas_, &layer);

        for (uint32_t y = 0; y < worldData.height; ++y) {
            for (uint32_t x = 0; x < worldData.width; ++x) {
                uint32_t idx = y * worldData.width + x;
                if (idx >= worldData.cells.size()) {
                    spdlog::error(
                        "CellRenderer: Cell index out of bounds (idx={}, size={})",
                        idx,
                        worldData.cells.size());
                    break;
                }
                const Cell& cell = worldData.cells[idx];

                // Calculate cell position with pre-computed offset
                int32_t cellX = renderOffsetX + x * scaledCellWidth_;
                int32_t cellY = renderOffsetY + y * scaledCellHeight_;

                renderCellDirectOptimized(cell, layer, cellX, cellY, debugDraw, false);
            }
        }

        lv_canvas_finish_layer(worldCanvas_, &layer);
    }
}

void CellRenderer::cleanup()
{
    spdlog::debug("CellRenderer: Cleaning up canvas");

    // Delete the fixed-size canvas (only called on final cleanup)
    if (worldCanvas_) {
        if (lv_obj_is_valid(worldCanvas_)) {
            lv_obj_del(worldCanvas_);
        }
        worldCanvas_ = nullptr;
    }

    // Clear the buffer
    canvasBuffer_.clear();
    canvasBuffer_.shrink_to_fit();

    canvasWidth_ = 0;
    canvasHeight_ = 0;
    width_ = 0;
    height_ = 0;
    parent_ = nullptr;
}

void CellRenderer::renderCellDirectOptimized(
    const Cell& cell,
    lv_layer_t& layer,
    int32_t cellX,
    int32_t cellY,
    bool debugDraw,
    bool usePixelRenderer)
{
    // Bounds check - skip cells outside canvas
    if (cellX < 0 || cellY < 0 || cellX + scaledCellWidth_ > canvasWidth_
        || cellY + scaledCellHeight_ > canvasHeight_) {
        return; // Silently skip out-of-bounds cells
    }

    // Branch between pixel renderer (fast) and LVGL renderer (slow but full-featured)
    if (usePixelRenderer) {
        // FAST PATH: Direct pixel buffer writes
        uint32_t* pixels = reinterpret_cast<uint32_t*>(canvasBuffer_.data());

        // Determine cell color
        uint32_t cellColor = 0xFF000000; // ARGB black with full alpha

        if (!cell.isEmpty() && cell.material_type != MaterialType::AIR) {
            lv_color_t matColor = getMaterialColor(cell.material_type);
            uint8_t alpha = static_cast<uint8_t>(cell.fill_ratio * 255.0 * 0.7);

            // Convert to ARGB32
            cellColor =
                (alpha << 24) | (matColor.red << 16) | (matColor.green << 8) | matColor.blue;
        }

        // Fill cell rectangle
        for (uint32_t py = 0; py < scaledCellHeight_; py++) {
            uint32_t rowStart = (cellY + py) * canvasWidth_ + cellX;
            for (uint32_t px = 0; px < scaledCellWidth_; px++) {
                pixels[rowStart + px] = cellColor;
            }
        }

        // TODO: Add pixel-based debug rendering if needed
        return;
    }

    // SLOW PATH: LVGL drawing (for comparison / debugging)

    // Black background for all cells
    lv_draw_rect_dsc_t bg_rect_dsc;
    lv_draw_rect_dsc_init(&bg_rect_dsc);
    bg_rect_dsc.bg_color = lv_color_hex(0x000000);
    bg_rect_dsc.bg_opa = LV_OPA_COVER;
    bg_rect_dsc.border_width = 0;

    lv_area_t bg_coords = { cellX,
                            cellY,
                            static_cast<int32_t>(cellX + scaledCellWidth_ - 1),
                            static_cast<int32_t>(cellY + scaledCellHeight_ - 1) };
    lv_draw_rect(&layer, &bg_rect_dsc, &bg_coords);

    // Render material if not empty
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
        rect_dsc.border_width = std::max(1, static_cast<int>(2 * scaleX_));
        rect_dsc.radius = scaledCellWidth_ > 5 ? std::max(1, static_cast<int>(2 * scaleX_)) : 0;

        lv_draw_rect(&layer, &rect_dsc, &bg_coords);

        // Debug features only if enabled and cells are large enough
        if (debugDraw && scaledCellWidth_ >= 8) {
            int com_pixel_x =
                cellX + static_cast<int>((cell.com.x + 1.0) * (scaledCellWidth_ - 1) / 2.0);
            int com_pixel_y =
                cellY + static_cast<int>((cell.com.y + 1.0) * (scaledCellHeight_ - 1) / 2.0);

            int square_size = std::max(2, static_cast<int>(6 * scaleX_));
            int half_size = square_size / 2;

            // COM indicator (yellow square)
            lv_draw_rect_dsc_t com_rect_dsc;
            lv_draw_rect_dsc_init(&com_rect_dsc);
            com_rect_dsc.bg_color = lv_color_hex(0xFFFF00);
            com_rect_dsc.bg_opa = LV_OPA_COVER;
            com_rect_dsc.border_color = lv_color_hex(0xCC9900);
            com_rect_dsc.border_opa = LV_OPA_COVER;
            com_rect_dsc.border_width = 1;
            com_rect_dsc.radius = 0;

            lv_area_t com_coords = { com_pixel_x - half_size,
                                     com_pixel_y - half_size,
                                     com_pixel_x + half_size - 1,
                                     com_pixel_y + half_size - 1 };
            lv_draw_rect(&layer, &com_rect_dsc, &com_coords);

            // Velocity vector (green line)
            if (scaledCellWidth_ >= 10 && cell.velocity.magnitude() > 0.01) {
                double scale = 20.0 * scaleX_;
                int end_x = com_pixel_x + static_cast<int>(cell.velocity.x * scale);
                int end_y = com_pixel_y + static_cast<int>(cell.velocity.y * scale);

                lv_draw_line_dsc_t line_dsc;
                lv_draw_line_dsc_init(&line_dsc);
                line_dsc.color = lv_color_hex(0x00FF00);
                line_dsc.width = std::max(1, static_cast<int>(2 * scaleX_));
                line_dsc.p1.x = com_pixel_x;
                line_dsc.p1.y = com_pixel_y;
                line_dsc.p2.x = end_x;
                line_dsc.p2.y = end_y;
                lv_draw_line(&layer, &line_dsc);
            }

            // Pressure visualization (borders showing magnitude).
            if (scaledCellWidth_ >= 10) {
                // Calculate border widths from pressure components.
                const double PRESSURE_BORDER_SCALE = 3.0;
                int dynamic_border_width = std::min(
                    static_cast<int>(cell.dynamic_component * PRESSURE_BORDER_SCALE * scaleX_),
                    static_cast<int>(scaledCellWidth_ / 3));
                int hydrostatic_border_width = std::min(
                    static_cast<int>(cell.hydrostatic_component * PRESSURE_BORDER_SCALE * scaleX_),
                    static_cast<int>(scaledCellWidth_ / 3));

                // Dynamic pressure border (magenta outer).
                if (dynamic_border_width > 0) {
                    lv_draw_rect_dsc_t dynamic_dsc;
                    lv_draw_rect_dsc_init(&dynamic_dsc);
                    dynamic_dsc.bg_opa = LV_OPA_TRANSP;
                    dynamic_dsc.border_color = lv_color_hex(0xFF00FF); // Magenta.
                    dynamic_dsc.border_opa = LV_OPA_COVER;
                    dynamic_dsc.border_width = dynamic_border_width;
                    dynamic_dsc.radius = 0;

                    lv_area_t dynamic_coords = { cellX, cellY,
                                                 cellX + static_cast<int>(scaledCellWidth_) - 1,
                                                 cellY + static_cast<int>(scaledCellHeight_) - 1 };
                    lv_draw_rect(&layer, &dynamic_dsc, &dynamic_coords);
                }

                // Hydrostatic pressure border (red inner).
                if (hydrostatic_border_width > 0) {
                    lv_draw_rect_dsc_t hydro_dsc;
                    lv_draw_rect_dsc_init(&hydro_dsc);
                    hydro_dsc.bg_opa = LV_OPA_TRANSP;
                    hydro_dsc.border_color = lv_color_hex(0xFF0000); // Red.
                    hydro_dsc.border_opa = LV_OPA_COVER;
                    hydro_dsc.border_width = hydrostatic_border_width;
                    hydro_dsc.radius = 0;

                    int inset = dynamic_border_width;
                    lv_area_t hydro_coords = { cellX + inset, cellY + inset,
                                               cellX + static_cast<int>(scaledCellWidth_) - 1 - inset,
                                               cellY + static_cast<int>(scaledCellHeight_) - 1 - inset };
                    lv_draw_rect(&layer, &hydro_dsc, &hydro_coords);
                }
            }

            // Pressure gradient vector (cyan line from center).
            if (scaledCellWidth_ >= 12 && cell.pressure_gradient.magnitude() > 0.001) {
                const double GRADIENT_SCALE = 15.0 * scaleX_;
                int end_x = com_pixel_x + static_cast<int>(cell.pressure_gradient.x * GRADIENT_SCALE);
                int end_y = com_pixel_y + static_cast<int>(cell.pressure_gradient.y * GRADIENT_SCALE);

                lv_draw_line_dsc_t grad_dsc;
                lv_draw_line_dsc_init(&grad_dsc);
                grad_dsc.color = lv_color_hex(0x00FFFF); // Cyan.
                grad_dsc.width = std::max(1, static_cast<int>(2 * scaleX_));
                grad_dsc.p1.x = com_pixel_x;
                grad_dsc.p1.y = com_pixel_y;
                grad_dsc.p2.x = end_x;
                grad_dsc.p2.y = end_y;
                lv_draw_line(&layer, &grad_dsc);
            }
        }
    }
}

} // namespace Ui
} // namespace DirtSim
