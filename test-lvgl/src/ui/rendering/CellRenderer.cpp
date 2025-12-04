#include "CellRenderer.h"
#include "core/MaterialType.h"
#include <algorithm>
#include <cassert>
#include <cmath>   // for std::round
#include <cstring> // for std::memcpy
#include <new>     // for std::bad_alloc
#include <spdlog/spdlog.h>

namespace DirtSim {
namespace Ui {

// Compile-time toggle for dithering in pixel renderer.
constexpr bool ENABLE_DITHERING = false;

// Mode-specific baseline scale factors (canvas size relative to container).
// Scale > 1.0 means canvas larger than container (downscaling = sharper).
// Scale < 1.0 means canvas smaller than container (upscaling = smoother).
// These baselines are multiplied by the user-adjustable scale factor.
constexpr double SCALE_BASELINE_SHARP = 1.0;  // 1:1 baseline for sharp mode.
constexpr double SCALE_BASELINE_SMOOTH = 0.6; // 40% smaller baseline for smooth upscale.
constexpr double SCALE_BASELINE_DEBUG = 1.3;  // 30% larger baseline for debug features.

// Global user-adjustable scale multiplier (affects all modes except PIXEL_PERFECT).
// Range: 0.1 (very smooth/blurry) to 2.0 (very sharp).
static double g_scaleFactorMultiplier = 0.4;

// 4x4 Bayer matrix for ordered dithering (values 0-15).
// Used to create stable, pattern-based transparency instead of alpha blending.
constexpr int BAYER_MATRIX_4X4[4][4] = {
    { 0, 8, 2, 10 }, { 12, 4, 14, 6 }, { 3, 11, 1, 9 }, { 15, 7, 13, 5 }
};

// Bresenham's line algorithm for fast pixel-based line drawing.
// Uses only integer math for maximum performance.
void drawLineBresenham(
    uint32_t* pixels,
    uint32_t canvasWidth,
    uint32_t canvasHeight,
    int x0,
    int y0,
    int x1,
    int y1,
    uint32_t color)
{
    int dx = std::abs(x1 - x0);
    int dy = std::abs(y1 - y0);
    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx - dy;

    while (true) {
        // Bounds check and plot pixel.
        if (x0 >= 0 && x0 < static_cast<int>(canvasWidth) && y0 >= 0
            && y0 < static_cast<int>(canvasHeight)) {
            pixels[y0 * canvasWidth + x0] = color;
        }
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x0 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y0 += sy;
        }
    }
}

// Calculate optimal pixels per cell based on world size, container size, and scale factor.
// The scale factor determines the ratio of canvas size to container size.
// Scale > 1.0 creates a larger canvas (downscaling = sharper).
// Scale < 1.0 creates a smaller canvas (upscaling = smoother).
static uint32_t calculateOptimalPixelsPerCell(
    uint32_t worldWidth,
    uint32_t worldHeight,
    int32_t containerWidth,
    int32_t containerHeight,
    double scaleFactor)
{
    if (containerWidth <= 0 || containerHeight <= 0 || worldWidth == 0 || worldHeight == 0) {
        return 8; // Fallback to reasonable default.
    }

    // Calculate target canvas size based on scale factor.
    double targetCanvasWidth = containerWidth * scaleFactor;
    double targetCanvasHeight = containerHeight * scaleFactor;

    // Calculate pixels per cell to achieve target canvas size.
    double pixelsPerCellX = targetCanvasWidth / worldWidth;
    double pixelsPerCellY = targetCanvasHeight / worldHeight;

    // Use smaller dimension to preserve aspect ratio.
    double pixelsPerCell = std::min(pixelsPerCellX, pixelsPerCellY);

    // Round to integer and clamp to reasonable bounds.
    uint32_t result = static_cast<uint32_t>(std::round(pixelsPerCell));
    uint32_t clamped = std::clamp(result, 4u, 32u); // Min 4px, max 32px per cell.

    spdlog::debug(
        "calculateOptimalPixelsPerCell: {}x{} world, {}x{} container, scale {:.2f} → {:.1f}px/cell "
        "(clamped to {}px)",
        worldWidth,
        worldHeight,
        containerWidth,
        containerHeight,
        scaleFactor,
        pixelsPerCell,
        clamped);

    return clamped;
}

// Get optimal pixel size for a given render mode.
// For PIXEL_PERFECT, returns 0 (special case - calculated dynamically).
static uint32_t getPixelsPerCellForMode(
    RenderMode mode,
    uint32_t worldWidth,
    uint32_t worldHeight,
    int32_t containerWidth,
    int32_t containerHeight)
{
    switch (mode) {
        case RenderMode::SHARP:
            return calculateOptimalPixelsPerCell(
                worldWidth,
                worldHeight,
                containerWidth,
                containerHeight,
                SCALE_BASELINE_SHARP * g_scaleFactorMultiplier);
        case RenderMode::SMOOTH:
            return calculateOptimalPixelsPerCell(
                worldWidth,
                worldHeight,
                containerWidth,
                containerHeight,
                SCALE_BASELINE_SMOOTH * g_scaleFactorMultiplier);
        case RenderMode::PIXEL_PERFECT:
            return 0; // Special: Calculate integer scale dynamically.
        case RenderMode::LVGL_DEBUG:
            return calculateOptimalPixelsPerCell(
                worldWidth,
                worldHeight,
                containerWidth,
                containerHeight,
                SCALE_BASELINE_DEBUG * g_scaleFactorMultiplier);
        case RenderMode::ADAPTIVE: {
            // Choose based on calculated cell size.
            uint32_t sharpSize = calculateOptimalPixelsPerCell(
                worldWidth,
                worldHeight,
                containerWidth,
                containerHeight,
                SCALE_BASELINE_SHARP * g_scaleFactorMultiplier);
            return (sharpSize < 4) ? calculateOptimalPixelsPerCell(
                                         worldWidth,
                                         worldHeight,
                                         containerWidth,
                                         containerHeight,
                                         SCALE_BASELINE_SMOOTH * g_scaleFactorMultiplier)
                                   : sharpSize;
        }
        default:
            return calculateOptimalPixelsPerCell(
                worldWidth,
                worldHeight,
                containerWidth,
                containerHeight,
                SCALE_BASELINE_SHARP * g_scaleFactorMultiplier);
    }
}

// Calculate integer-only pixels per cell that fits in container.
// Returns largest integer where (worldSize × pixels) fits in container.
static uint32_t calculateIntegerPixelsPerCell(
    uint32_t worldWidth, uint32_t worldHeight, int32_t containerWidth, int32_t containerHeight)
{
    // Calculate max integer scale for each dimension.
    uint32_t maxScaleX = (containerWidth > 0) ? containerWidth / worldWidth : 1;
    uint32_t maxScaleY = (containerHeight > 0) ? containerHeight / worldHeight : 1;

    // Use smaller scale to fit both dimensions.
    uint32_t scale = std::min(maxScaleX, maxScaleY);

    // Ensure minimum of 2px per cell.
    return std::max(2u, scale);
}

// Apply bilinear smoothing filter to blend adjacent pixels.
// This creates anti-aliasing at cell boundaries.
static void applyBilinearFilter(uint32_t* pixels, uint32_t width, uint32_t height)
{
    if (width < 2 || height < 2) return;

    // Create temporary buffer for filtered output.
    std::vector<uint32_t> filtered(width * height);

    // Apply 2x2 box filter to smooth transitions.
    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            uint32_t idx = y * width + x;

            // Sample neighborhood (with boundary clamping).
            uint32_t x0 = x;
            uint32_t x1 = std::min(x + 1, width - 1);
            uint32_t y0 = y;
            uint32_t y1 = std::min(y + 1, height - 1);

            // Get four samples.
            uint32_t p00 = pixels[y0 * width + x0];
            uint32_t p10 = pixels[y0 * width + x1];
            uint32_t p01 = pixels[y1 * width + x0];
            uint32_t p11 = pixels[y1 * width + x1];

            // Extract and average ARGB channels.
            uint32_t a = ((p00 >> 24) + (p10 >> 24) + (p01 >> 24) + (p11 >> 24)) / 4;
            uint32_t r = (((p00 >> 16) & 0xFF) + ((p10 >> 16) & 0xFF) + ((p01 >> 16) & 0xFF)
                          + ((p11 >> 16) & 0xFF))
                / 4;
            uint32_t g = (((p00 >> 8) & 0xFF) + ((p10 >> 8) & 0xFF) + ((p01 >> 8) & 0xFF)
                          + ((p11 >> 8) & 0xFF))
                / 4;
            uint32_t b = ((p00 & 0xFF) + (p10 & 0xFF) + (p01 & 0xFF) + (p11 & 0xFF)) / 4;

            filtered[idx] = (a << 24) | (r << 16) | (g << 8) | b;
        }
    }

    // Copy filtered result back to original buffer.
    std::memcpy(pixels, filtered.data(), width * height * sizeof(uint32_t));
}

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
        case MaterialType::ROOT:
            return lv_color_hex(0xDEB887); // Burlywood.
        case MaterialType::SAND:
            return lv_color_hex(0xFFB347); // Sandy orange.
        case MaterialType::SEED:
            return lv_color_hex(0xFFD700); // Gold (bright and distinctive).
        case MaterialType::WALL:
            return lv_color_hex(0x808080); // Gray.
        case MaterialType::WATER:
            return lv_color_hex(0x00BFFF); // Deep sky blue.
        case MaterialType::WOOD:
            return lv_color_hex(0x654321); // Dark brown.
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

    // Round to nearest integer to maximize canvas usage.
    uint32_t candidateCellSize = static_cast<uint32_t>(std::round(pixelsPerCell));

    // Ensure the total rendering size fits within canvas bounds.
    // If rounding up would exceed canvas, use floor instead.
    if (candidateCellSize * worldWidth > canvasWidth_
        || candidateCellSize * worldHeight > canvasHeight_) {
        candidateCellSize = static_cast<uint32_t>(std::floor(pixelsPerCell));
    }

    scaledCellWidth_ = candidateCellSize;
    scaledCellHeight_ = candidateCellSize;

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
    // Use default scale factor to calculate initial pixel size.
    int32_t containerWidth = lv_obj_get_width(parent);
    int32_t containerHeight = lv_obj_get_height(parent);
    uint32_t pixelsPerCell = calculateOptimalPixelsPerCell(
        worldWidth,
        worldHeight,
        containerWidth,
        containerHeight,
        SCALE_BASELINE_SHARP * g_scaleFactorMultiplier);
    initializeWithPixelSize(parent, worldWidth, worldHeight, pixelsPerCell);
}

void CellRenderer::initializeWithPixelSize(
    lv_obj_t* parent, uint32_t worldWidth, uint32_t worldHeight, uint32_t pixelsPerCell)
{
    spdlog::info(
        "CellRenderer: Initializing canvas with transform scaling ({}px/cell)", pixelsPerCell);

    // Validate input parameters.
    if (!parent) {
        spdlog::error("CellRenderer: Invalid parent for initialization");
        return;
    }

    // Only initialize once - canvas stays fixed size.
    if (worldCanvas_) {
        spdlog::debug("CellRenderer: Canvas already initialized, skipping");
        return;
    }

    parent_ = parent;
    width_ = worldWidth;
    height_ = worldHeight;

    // Get container size for transform scaling calculations.
    int32_t containerWidth = lv_obj_get_width(parent);
    int32_t containerHeight = lv_obj_get_height(parent);

    // Store container size for resize detection.
    lastContainerWidth_ = containerWidth;
    lastContainerHeight_ = containerHeight;

    // Sanity check container dimensions.
    if (containerWidth <= 0 || containerHeight <= 0) {
        spdlog::warn(
            "CellRenderer: Invalid container dimensions {}x{}, using defaults",
            containerWidth,
            containerHeight);
        containerWidth = 800;
        containerHeight = 600;
    }

    // Render at world dimensions × pixelsPerCell.
    // This gives us a native resolution canvas that we'll scale to fit the container.
    canvasWidth_ = worldWidth * pixelsPerCell;
    canvasHeight_ = worldHeight * pixelsPerCell;

    // Each cell gets exactly pixelsPerCell pixels.
    scaledCellWidth_ = pixelsPerCell;
    scaledCellHeight_ = pixelsPerCell;

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

    // Set canvas buffer (this never changes).
    lv_canvas_set_buffer(
        worldCanvas_, canvasBuffer_.data(), canvasWidth_, canvasHeight_, LV_COLOR_FORMAT_ARGB8888);

    // Position canvas at top-left of container.
    lv_obj_set_pos(worldCanvas_, 0, 0);

    // Apply LVGL transform scaling to fit canvas to container.
    // LVGL uses fixed-point scaling where 256 = 1.0×.
    // Calculate scale to fit world in container while preserving aspect ratio.
    double scaleX = (double)containerWidth / canvasWidth_;
    double scaleY = (double)containerHeight / canvasHeight_;
    double scale = std::min(scaleX, scaleY); // Preserve aspect ratio.

    int lvglScaleX = (int)(scale * 256);
    int lvglScaleY = (int)(scale * 256);

    lv_obj_set_style_transform_scale_x(worldCanvas_, lvglScaleX, 0);
    lv_obj_set_style_transform_scale_y(worldCanvas_, lvglScaleY, 0);

    spdlog::info(
        "CellRenderer: Initialized canvas {}x{} pixels ({}x{} cells at {}px/cell), scaling {:.2f}×",
        canvasWidth_,
        canvasHeight_,
        worldWidth,
        worldHeight,
        pixelsPerCell,
        scale);
}

void CellRenderer::resize(lv_obj_t* parent, uint32_t worldWidth, uint32_t worldHeight)
{
    spdlog::info(
        "CellRenderer: Updating world size from {}x{} to {}x{}",
        width_,
        height_,
        worldWidth,
        worldHeight);

    // Only update if dimensions actually changed.
    if (width_ == worldWidth && height_ == worldHeight && parent_ == parent) {
        spdlog::debug("CellRenderer: No size change, skipping");
        return;
    }

    // World size change requires canvas reallocation with transform scaling.
    // Clean up and reinitialize.
    cleanup();
    initialize(parent, worldWidth, worldHeight);
}

void CellRenderer::renderWorldData(
    const WorldData& worldData, lv_obj_t* parent, bool debugDraw, RenderMode mode)
{
    // Validate input.
    if (!parent || worldData.width == 0 || worldData.height == 0) {
        spdlog::warn(
            "CellRenderer: Invalid render parameters (parent={}, size={}x{})",
            (void*)parent,
            worldData.width,
            worldData.height);
        return;
    }

    // Resolve adaptive mode to concrete mode based on cell size.
    RenderMode effectiveMode = mode;
    if (mode == RenderMode::ADAPTIVE) {
        // Choose SMOOTH for small cells (<4px), SHARP for larger cells.
        effectiveMode = (scaledCellWidth_ < 4) ? RenderMode::SMOOTH : RenderMode::SHARP;
    }

    // Get container dimensions for calculations.
    int32_t currentContainerWidth = lv_obj_get_width(parent);
    int32_t currentContainerHeight = lv_obj_get_height(parent);

    // Check if mode changed and requires different pixel size.
    uint32_t requiredPixelSize = getPixelsPerCellForMode(
        effectiveMode,
        worldData.width,
        worldData.height,
        currentContainerWidth,
        currentContainerHeight);

    // Check if reinitialization is needed due to mode change or pixel size change.
    bool modeChanged = (effectiveMode != currentMode_);
    bool pixelSizeChanged = (scaledCellWidth_ != requiredPixelSize);
    bool needsReinitialization =
        modeChanged || pixelSizeChanged || (effectiveMode == RenderMode::PIXEL_PERFECT);

    if (worldCanvas_ && needsReinitialization) {
        spdlog::info(
            "CellRenderer: Mode changed from {} to {}, reinitializing",
            renderModeToString(currentMode_),
            renderModeToString(effectiveMode));
        cleanup();
    }

    currentMode_ = effectiveMode;

    // Determine rendering path based on mode.
    bool usePixelRenderer = (effectiveMode != RenderMode::LVGL_DEBUG);
    bool useBilinearFilter = (effectiveMode == RenderMode::SMOOTH);

    // If container size changed significantly, reinitialize canvas.
    const int32_t RESIZE_THRESHOLD = 50; // Avoid jitter from small changes.
    bool containerResized =
        (std::abs(currentContainerWidth - lastContainerWidth_) > RESIZE_THRESHOLD)
        || (std::abs(currentContainerHeight - lastContainerHeight_) > RESIZE_THRESHOLD);

    if (containerResized && worldCanvas_) {
        spdlog::info(
            "CellRenderer: Container resized from {}x{} to {}x{}, reinitializing canvas",
            lastContainerWidth_,
            lastContainerHeight_,
            currentContainerWidth,
            currentContainerHeight);
        cleanup();
    }

    // Initialize canvas on first call or after resize/mode change.
    if (!worldCanvas_) {
        // Use mode-specific pixel size for optimal quality.
        uint32_t pixelsPerCell = getPixelsPerCellForMode(
            effectiveMode,
            worldData.width,
            worldData.height,
            currentContainerWidth,
            currentContainerHeight);

        // PIXEL_PERFECT mode calculates integer scale dynamically.
        if (effectiveMode == RenderMode::PIXEL_PERFECT) {
            pixelsPerCell = calculateIntegerPixelsPerCell(
                worldData.width, worldData.height, currentContainerWidth, currentContainerHeight);
            spdlog::info(
                "CellRenderer: PIXEL_PERFECT mode - using {}× integer scale", pixelsPerCell);
        }

        initializeWithPixelSize(parent, worldData.width, worldData.height, pixelsPerCell);
        if (!worldCanvas_) {
            return; // Failed to initialize.
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

    // With transform scaling, world fills canvas exactly - no offset needed.
    int32_t renderOffsetX = 0;
    int32_t renderOffsetY = 0;

    if (usePixelRenderer) {
        // FAST PATH: Direct pixel rendering with alpha blending
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

                // Prepare border color and interior color.
                uint32_t borderColor = 0xFF000000;   // ARGB black with full alpha.
                uint32_t interiorColor = 0xFF000000; // ARGB black with full alpha.

                if (!cell.isEmpty() && cell.material_type != MaterialType::AIR) {
                    lv_color_t matColor = getMaterialColor(cell.material_type);
                    // Border opacity varies by debug mode.
                    // Debug mode: full opacity (pronounced border).
                    // Normal mode: 0.85 opacity (subtle/faint border).
                    double borderOpacityFactor = debugDraw ? 1.0 : 0.85;
                    uint8_t borderAlpha =
                        static_cast<uint8_t>(cell.fill_ratio * 255.0 * borderOpacityFactor);
                    // Interior always at 0.7 opacity (darker).
                    uint8_t interiorAlpha = static_cast<uint8_t>(cell.fill_ratio * 255.0 * 0.7);

                    borderColor = (borderAlpha << 24) | (matColor.red << 16) | (matColor.green << 8)
                        | matColor.blue;
                    interiorColor = (interiorAlpha << 24) | (matColor.red << 16)
                        | (matColor.green << 8) | matColor.blue;
                }

                // Fill cell rectangle with border and interior.
                for (uint32_t py = 0; py < scaledCellHeight_; py++) {
                    uint32_t rowStart = (cellY + py) * canvasWidth_ + cellX;
                    for (uint32_t px = 0; px < scaledCellWidth_; px++) {
                        uint32_t pixelIdx = rowStart + px;

                        // Determine if this pixel is on the border.
                        bool isBorder =
                            (px == 0 || px == scaledCellWidth_ - 1 || py == 0
                             || py == scaledCellHeight_ - 1);

                        // Select color based on position (border vs interior).
                        uint32_t pixelColor = isBorder ? borderColor : interiorColor;
                        uint8_t alpha = (pixelColor >> 24) & 0xFF;

                        if constexpr (ENABLE_DITHERING) {
                            // Dithered rendering: use Bayer matrix to decide pixel on/off.
                            if (alpha == 0) {
                                // Fully transparent - skip.
                                continue;
                            }
                            else if (alpha == 255) {
                                // Fully opaque - direct write.
                                pixels[pixelIdx] = pixelColor;
                            }
                            else {
                                // Partial transparency - use dithering.
                                // Get Bayer threshold for this pixel position.
                                int bayerX = (cellX + px) % 4;
                                int bayerY = (cellY + py) % 4;
                                int bayerThreshold = BAYER_MATRIX_4X4[bayerY][bayerX];

                                // Compare alpha to threshold (scaled 0-255 to 0-15).
                                // If alpha > threshold, draw pixel at full opacity.
                                if ((alpha * 16 / 256) > bayerThreshold) {
                                    // Draw pixel with full material color (no alpha).
                                    pixels[pixelIdx] = 0xFF000000 | (pixelColor & 0x00FFFFFF);
                                }
                                // Otherwise skip pixel (leave background).
                            }
                        }
                        else {
                            // Alpha blending: blend source with destination
                            if (alpha == 0) {
                                // Fully transparent - skip (keep background)
                                continue;
                            }
                            else if (alpha == 255) {
                                // Fully opaque - direct write (optimization)
                                pixels[pixelIdx] = pixelColor;
                            }
                            else {
                                // Partial transparency - blend
                                uint32_t dstColor = pixels[pixelIdx];
                                uint8_t invAlpha = 255 - alpha;

                                // Extract source channels
                                uint8_t srcR = (pixelColor >> 16) & 0xFF;
                                uint8_t srcG = (pixelColor >> 8) & 0xFF;
                                uint8_t srcB = pixelColor & 0xFF;

                                // Extract destination channels
                                uint8_t dstR = (dstColor >> 16) & 0xFF;
                                uint8_t dstG = (dstColor >> 8) & 0xFF;
                                uint8_t dstB = dstColor & 0xFF;

                                // Blend: result = (src * alpha + dst * (1 - alpha)) / 255
                                uint8_t r = (srcR * alpha + dstR * invAlpha) / 255;
                                uint8_t g = (srcG * alpha + dstG * invAlpha) / 255;
                                uint8_t b = (srcB * alpha + dstB * invAlpha) / 255;

                                // Write blended pixel (with full alpha)
                                pixels[pixelIdx] = 0xFF000000 | (r << 16) | (g << 8) | b;
                            }
                        }
                    }
                }

                // Debug draw: Pressure visualization (drawn first, under COM/vectors).
                if (debugDraw && !cell.isEmpty() && cell.material_type != MaterialType::AIR) {
                    // Pressure visualization (fixed-width borders with variable opacity).
                    if (scaledCellWidth_ >= 4) {
                        // Calculate opacity from unified pressure.
                        const double PRESSURE_OPACITY_SCALE = 25.0;
                        int pressure_opacity =
                            std::min(static_cast<int>(cell.pressure * PRESSURE_OPACITY_SCALE), 255);

                        // Fixed border widths.
                        const int FIXED_BORDER_WIDTH = std::max(1, static_cast<int>(2 * scaleX_));

                        // Unified pressure border (cyan).
                        if (pressure_opacity > 0) {
                            uint32_t cyan_color = 0x00FFFF;
                            uint8_t alpha = static_cast<uint8_t>(pressure_opacity * 0.5);

                            for (uint32_t py = 0; py < scaledCellHeight_; py++) {
                                for (uint32_t px = 0; px < scaledCellWidth_; px++) {
                                    bool is_border =
                                        (px < static_cast<uint32_t>(FIXED_BORDER_WIDTH)
                                         || px >= scaledCellWidth_
                                                 - static_cast<uint32_t>(FIXED_BORDER_WIDTH)
                                         || py < static_cast<uint32_t>(FIXED_BORDER_WIDTH)
                                         || py >= scaledCellHeight_
                                                 - static_cast<uint32_t>(FIXED_BORDER_WIDTH));

                                    if (is_border) {
                                        uint32_t pixelIdx =
                                            (cellY + py) * canvasWidth_ + (cellX + px);
                                        uint32_t src_color = (alpha << 24) | cyan_color;

                                        uint32_t dst_color = pixels[pixelIdx];
                                        uint8_t inv_alpha = 255 - alpha;

                                        uint8_t src_r = (src_color >> 16) & 0xFF;
                                        uint8_t src_g = (src_color >> 8) & 0xFF;
                                        uint8_t src_b = src_color & 0xFF;

                                        uint8_t dst_r = (dst_color >> 16) & 0xFF;
                                        uint8_t dst_g = (dst_color >> 8) & 0xFF;
                                        uint8_t dst_b = dst_color & 0xFF;

                                        uint8_t r = (src_r * alpha + dst_r * inv_alpha) / 255;
                                        uint8_t g = (src_g * alpha + dst_g * inv_alpha) / 255;
                                        uint8_t b = (src_b * alpha + dst_b * inv_alpha) / 255;

                                        pixels[pixelIdx] = 0xFF000000 | (r << 16) | (g << 8) | b;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        // Debug draw: Support indicators, vectors, then COM last (so COM is never obscured).
        if (debugDraw) {
            uint32_t cells_with_support = 0;
            uint32_t non_air_cells = 0;

            // First pass: Draw support indicators (small corner dots).
            const int SUPPORT_DOT_SIZE = std::max(3, static_cast<int>(3 * scaleX_));
            for (uint32_t y = 0; y < worldData.height; ++y) {
                for (uint32_t x = 0; x < worldData.width; ++x) {
                    uint32_t idx = y * worldData.width + x;
                    if (idx >= worldData.cells.size()) break;

                    const Cell& cell = worldData.cells[idx];
                    if (cell.isEmpty() || cell.material_type == MaterialType::AIR) continue;

                    non_air_cells++;
                    if (cell.has_any_support) {
                        cells_with_support++;
                    }

                    int32_t cellX = x * scaledCellWidth_;
                    int32_t cellY = y * scaledCellHeight_;

                    // Draw support indicator in top-left corner.
                    if (cell.has_any_support) {
                        spdlog::info(
                            "Support visualization: Cell at ({},{}) has_any_support=true", x, y);
                        // Green dot for supported cells.
                        uint32_t support_color = 0xFF00FF00; // ARGB: green.

                        // Draw small square in top-left corner.
                        for (int dy = 0; dy < SUPPORT_DOT_SIZE; dy++) {
                            for (int dx = 0; dx < SUPPORT_DOT_SIZE; dx++) {
                                int px = cellX + dx + 1; // +1 to avoid cell border.
                                int py = cellY + dy + 1;
                                if (px >= 0 && px < static_cast<int>(canvasWidth_) && py >= 0
                                    && py < static_cast<int>(canvasHeight_)) {
                                    pixels[py * canvasWidth_ + px] = support_color;
                                }
                            }
                        }

                        // If vertical support specifically, add a second indicator.
                        if (cell.has_vertical_support) {
                            spdlog::info(
                                "Support visualization: Cell at ({},{}) has_vertical_support=true",
                                x,
                                y);
                            // Brighter green for vertical support (bottom-left corner).
                            uint32_t vertical_color = 0xFF00FF00; // Same green, could be different.
                            int bottom_y = cellY + scaledCellHeight_ - SUPPORT_DOT_SIZE - 1;
                            for (int dy = 0; dy < SUPPORT_DOT_SIZE; dy++) {
                                for (int dx = 0; dx < SUPPORT_DOT_SIZE; dx++) {
                                    int px = cellX + dx + 1;
                                    int py = bottom_y + dy;
                                    if (px >= 0 && px < static_cast<int>(canvasWidth_) && py >= 0
                                        && py < static_cast<int>(canvasHeight_)) {
                                        pixels[py * canvasWidth_ + px] = vertical_color;
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // Second pass: Draw pressure gradient vectors.
            for (uint32_t y = 0; y < worldData.height; ++y) {
                for (uint32_t x = 0; x < worldData.width; ++x) {
                    uint32_t idx = y * worldData.width + x;
                    if (idx >= worldData.cells.size()) break;

                    const Cell& cell = worldData.cells[idx];
                    if (cell.isEmpty() || cell.material_type == MaterialType::AIR) continue;

                    int32_t cellX = x * scaledCellWidth_;
                    int32_t cellY = y * scaledCellHeight_;

                    // Calculate COM position in pixel coordinates.
                    // COM ranges from [-1, 1] where -1 is top/left and +1 is bottom/right.
                    int com_pixel_x =
                        cellX + static_cast<int>((cell.com.x + 1.0) * (scaledCellWidth_ - 1) / 2.0);
                    int com_pixel_y = cellY
                        + static_cast<int>((cell.com.y + 1.0) * (scaledCellHeight_ - 1) / 2.0);

                    // Pressure gradient vector (cyan line from COM).
                    if (cell.pressure_gradient.magnitude() > 0.001) {
                        const double GRADIENT_SCALE = scaleX_;
                        const int end_x = com_pixel_x
                            + static_cast<int>(cell.pressure_gradient.x * GRADIENT_SCALE);
                        const int end_y = com_pixel_y
                            + static_cast<int>(cell.pressure_gradient.y * GRADIENT_SCALE);
                        drawLineBresenham(
                            pixels,
                            canvasWidth_,
                            canvasHeight_,
                            com_pixel_x,
                            com_pixel_y,
                            end_x,
                            end_y,
                            0xFF00FFFF); // Cyan.
                    }
                }
            }

            // Third pass: Draw COM indicators (absolute last, never obscured).
            for (uint32_t y = 0; y < worldData.height; ++y) {
                for (uint32_t x = 0; x < worldData.width; ++x) {
                    uint32_t idx = y * worldData.width + x;
                    if (idx >= worldData.cells.size()) break;

                    const Cell& cell = worldData.cells[idx];
                    if (cell.isEmpty() || cell.material_type == MaterialType::AIR) continue;

                    int32_t cellX = x * scaledCellWidth_;
                    int32_t cellY = y * scaledCellHeight_;

                    // Calculate COM position in pixel coordinates.
                    // COM ranges from [-1, 1] where -1 is top/left and +1 is bottom/right.
                    int com_pixel_x =
                        cellX + static_cast<int>((cell.com.x + 1.0) * (scaledCellWidth_ - 1) / 2.0);
                    int com_pixel_y = cellY
                        + static_cast<int>((cell.com.y + 1.0) * (scaledCellHeight_ - 1) / 2.0);

                    // Bounds check.
                    if (com_pixel_x >= 0 && com_pixel_x < static_cast<int>(canvasWidth_)
                        && com_pixel_y >= 0 && com_pixel_y < static_cast<int>(canvasHeight_)) {
                        uint32_t comPixelIdx = com_pixel_y * canvasWidth_ + com_pixel_x;
                        // Yellow pixel for COM (same as LVGL debug draw).
                        pixels[comPixelIdx] = 0xFFFFFF00; // ARGB: full alpha, yellow.
                    }
                }
            }
        }

        // Debug draw: Bone connections (white lines showing organism structure).
        if (debugDraw && !worldData.bones.empty()) {
            for (const auto& bone : worldData.bones) {
                // Calculate cell centers for bone endpoints.
                int32_t cell_a_x = bone.cell_a.x * scaledCellWidth_ + scaledCellWidth_ / 2;
                int32_t cell_a_y = bone.cell_a.y * scaledCellHeight_ + scaledCellHeight_ / 2;
                int32_t cell_b_x = bone.cell_b.x * scaledCellWidth_ + scaledCellWidth_ / 2;
                int32_t cell_b_y = bone.cell_b.y * scaledCellHeight_ + scaledCellHeight_ / 2;

                // Draw bone as white line.
                drawLineBresenham(
                    pixels,
                    canvasWidth_,
                    canvasHeight_,
                    cell_a_x,
                    cell_a_y,
                    cell_b_x,
                    cell_b_y,
                    0xFFFFFFFF); // White.
            }
        }

        // Apply bilinear smoothing filter if mode requires it.
        if (useBilinearFilter) {
            applyBilinearFilter(pixels, canvasWidth_, canvasHeight_);
        }

        // Invalidate canvas to trigger display update.
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
                const CellDebug& debug = worldData.debug_info[idx];

                // Calculate cell position with pre-computed offset
                int32_t cellX = renderOffsetX + x * scaledCellWidth_;
                int32_t cellY = renderOffsetY + y * scaledCellHeight_;

                renderCellLVGL(cell, debug, layer, cellX, cellY, debugDraw);
            }
        }

        lv_canvas_finish_layer(worldCanvas_, &layer);
    }
}

void CellRenderer::cleanup()
{
    spdlog::debug("CellRenderer: Cleaning up canvas");

    // Delete the fixed-size canvas (only called on final cleanup).
    if (worldCanvas_) {
        if (lv_obj_is_valid(worldCanvas_)) {
            lv_obj_del(worldCanvas_);
        }
        worldCanvas_ = nullptr;
    }

    // Clear the buffer.
    canvasBuffer_.clear();
    canvasBuffer_.shrink_to_fit();

    canvasWidth_ = 0;
    canvasHeight_ = 0;
    width_ = 0;
    height_ = 0;
    parent_ = nullptr;
    lastContainerWidth_ = 0;
    lastContainerHeight_ = 0;
}

void CellRenderer::renderCellLVGL(
    const Cell& cell,
    const CellDebug& debug,
    lv_layer_t& layer,
    int32_t cellX,
    int32_t cellY,
    bool debugDraw)
{
    // Bounds check - skip cells outside canvas.
    if (cellX < 0 || cellY < 0 || cellX + scaledCellWidth_ > canvasWidth_
        || cellY + scaledCellHeight_ > canvasHeight_) {
        return;
    }

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

            // Support indicators (green dots in corners).
            const int SUPPORT_DOT_SIZE = std::max(2, static_cast<int>(3 * scaleX_));
            if (cell.has_any_support) {
                // Top-left corner - any support.
                lv_draw_rect_dsc_t support_dsc;
                lv_draw_rect_dsc_init(&support_dsc);
                support_dsc.bg_color = lv_color_hex(0x00FF00); // Green.
                support_dsc.bg_opa = LV_OPA_COVER;
                support_dsc.border_width = 0;
                support_dsc.radius = 0;

                lv_area_t support_coords = {
                    cellX + 1, cellY + 1, cellX + SUPPORT_DOT_SIZE, cellY + SUPPORT_DOT_SIZE
                };
                lv_draw_rect(&layer, &support_dsc, &support_coords);

                // Bottom-left corner - vertical support.
                if (cell.has_vertical_support) {
                    lv_area_t vertical_coords = { cellX + 1,
                                                  cellY + static_cast<int>(scaledCellHeight_)
                                                      - SUPPORT_DOT_SIZE - 1,
                                                  cellX + SUPPORT_DOT_SIZE,
                                                  cellY + static_cast<int>(scaledCellHeight_) - 1 };
                    lv_draw_rect(&layer, &support_dsc, &vertical_coords);
                }
            }

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
                const double scale = 1.0 * scaleX_;
                const int end_x = com_pixel_x + static_cast<int>(cell.velocity.x * scale);
                const int end_y = com_pixel_y + static_cast<int>(cell.velocity.y * scale);

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

            // Pressure visualization (fixed-width borders with variable opacity).
            if (scaledCellWidth_ >= 10) {
                // Fixed border widths.
                const int FIXED_BORDER_WIDTH = std::max(1, static_cast<int>(2 * scaleX_));

                // Calculate opacity from unified pressure.
                const double PRESSURE_OPACITY_SCALE = 25.0;
                int pressure_opacity =
                    std::min(static_cast<int>(cell.pressure * PRESSURE_OPACITY_SCALE), 255);

                // Unified pressure border (cyan).
                if (pressure_opacity > 0) {
                    lv_draw_rect_dsc_t pressure_dsc;
                    lv_draw_rect_dsc_init(&pressure_dsc);
                    pressure_dsc.bg_opa = LV_OPA_TRANSP;
                    pressure_dsc.border_color = lv_color_hex(0x00FFFF);
                    pressure_dsc.border_opa = static_cast<lv_opa_t>(pressure_opacity);
                    pressure_dsc.border_width = FIXED_BORDER_WIDTH;
                    pressure_dsc.radius = 0;

                    lv_area_t pressure_coords = { cellX,
                                                  cellY,
                                                  cellX + static_cast<int>(scaledCellWidth_) - 1,
                                                  cellY + static_cast<int>(scaledCellHeight_) - 1 };
                    lv_draw_rect(&layer, &pressure_dsc, &pressure_coords);
                }
            }

            // Pressure gradient vector (cyan line from center).
            if (scaledCellWidth_ >= 12 && cell.pressure_gradient.magnitude() > 0.001) {
                const double GRADIENT_SCALE = 10 * scaleX_;
                int end_x =
                    com_pixel_x + static_cast<int>(cell.pressure_gradient.x * GRADIENT_SCALE);
                int end_y =
                    com_pixel_y + static_cast<int>(cell.pressure_gradient.y * GRADIENT_SCALE);

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

            // Adhesion force vector (orange line from center).
            if (scaledCellWidth_ >= 10 && debug.accumulated_adhesion_force.magnitude() > 0.01) {
                const double ADHESION_SCALE = 10.0 * scaleX_;
                int end_x = com_pixel_x
                    + static_cast<int>(debug.accumulated_adhesion_force.x * ADHESION_SCALE);
                int end_y = com_pixel_y
                    + static_cast<int>(debug.accumulated_adhesion_force.y * ADHESION_SCALE);

                lv_draw_line_dsc_t adhesion_dsc;
                lv_draw_line_dsc_init(&adhesion_dsc);
                adhesion_dsc.color = lv_color_hex(0xFF8000); // Orange.
                adhesion_dsc.width = std::max(1, static_cast<int>(2 * scaleX_));
                adhesion_dsc.p1.x = com_pixel_x;
                adhesion_dsc.p1.y = com_pixel_y;
                adhesion_dsc.p2.x = end_x;
                adhesion_dsc.p2.y = end_y;
                lv_draw_line(&layer, &adhesion_dsc);
            }

            // COM cohesion force vector (purple line from cell center).
            if (scaledCellWidth_ >= 10 && debug.accumulated_com_cohesion_force.magnitude() > 0.01) {
                const double COHESION_SCALE = 1.0 * scaleX_;

                // Draw from cell center (not COM).
                int cell_center_x = cellX + scaledCellWidth_ / 2;
                int cell_center_y = cellY + scaledCellHeight_ / 2;

                int end_x = cell_center_x
                    + static_cast<int>(debug.accumulated_com_cohesion_force.x * COHESION_SCALE);
                int end_y = cell_center_y
                    + static_cast<int>(debug.accumulated_com_cohesion_force.y * COHESION_SCALE);

                lv_draw_line_dsc_t cohesion_dsc;
                lv_draw_line_dsc_init(&cohesion_dsc);
                cohesion_dsc.color = lv_color_hex(0x9370DB); // Purple (medium purple).
                cohesion_dsc.width = std::max(1, static_cast<int>(2 * scaleX_));
                cohesion_dsc.p1.x = cell_center_x;
                cohesion_dsc.p1.y = cell_center_y;
                cohesion_dsc.p2.x = end_x;
                cohesion_dsc.p2.y = end_y;
                lv_draw_line(&layer, &cohesion_dsc);
            }

            // Viscous force vector (cyan line from cell center).
            if (scaledCellWidth_ >= 10 && debug.accumulated_viscous_force.magnitude() > 0.01) {
                const double VISCOUS_SCALE = 5.0 * scaleX_;

                // Draw from cell center.
                int cell_center_x = cellX + scaledCellWidth_ / 2;
                int cell_center_y = cellY + scaledCellHeight_ / 2;

                int end_x = cell_center_x
                    + static_cast<int>(debug.accumulated_viscous_force.x * VISCOUS_SCALE);
                int end_y = cell_center_y
                    + static_cast<int>(debug.accumulated_viscous_force.y * VISCOUS_SCALE);

                lv_draw_line_dsc_t viscous_dsc;
                lv_draw_line_dsc_init(&viscous_dsc);
                viscous_dsc.color = lv_color_hex(0x00FFFF); // Cyan.
                viscous_dsc.width = std::max(1, static_cast<int>(2 * scaleX_));
                viscous_dsc.p1.x = cell_center_x;
                viscous_dsc.p1.y = cell_center_y;
                viscous_dsc.p2.x = end_x;
                viscous_dsc.p2.y = end_y;
                lv_draw_line(&layer, &viscous_dsc);
            }
        }
    }
}

double getSharpScaleFactor()
{
    return g_scaleFactorMultiplier;
}

void setSharpScaleFactor(double scaleFactor)
{
    g_scaleFactorMultiplier = std::clamp(scaleFactor, 0.01, 2.0); // Clamp to reasonable range.
}

} // namespace Ui
} // namespace DirtSim
