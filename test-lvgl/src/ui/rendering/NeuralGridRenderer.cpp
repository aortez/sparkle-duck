#include "NeuralGridRenderer.h"
#include "core/MaterialType.h"
#include <algorithm>
#include <cmath>
#include <spdlog/spdlog.h>

namespace DirtSim {
namespace Ui {

NeuralGridRenderer::~NeuralGridRenderer()
{
    cleanup();
}

void NeuralGridRenderer::initialize(lv_obj_t* parent)
{
    if (!parent) {
        spdlog::error("NeuralGridRenderer::initialize: parent is null");
        return;
    }

    cleanup();

    // Allocate canvas buffer (ARGB8888 format: 4 bytes per pixel).
    const size_t bufferSize = CANVAS_WIDTH * CANVAS_HEIGHT * 4;
    canvasBuffer_.resize(bufferSize);

    // Create canvas.
    gridCanvas_ = lv_canvas_create(parent);
    if (!gridCanvas_) {
        spdlog::error("NeuralGridRenderer::initialize: Failed to create canvas");
        return;
    }

    lv_canvas_set_buffer(
        gridCanvas_, canvasBuffer_.data(), CANVAS_WIDTH, CANVAS_HEIGHT, LV_COLOR_FORMAT_ARGB8888);

    // Center canvas in parent.
    lv_obj_center(gridCanvas_);

    // Clear to dark background.
    lv_canvas_fill_bg(gridCanvas_, lv_color_hex(0x202020), LV_OPA_COVER);

    spdlog::info("NeuralGridRenderer: Initialized {}x{} grid canvas", GRID_SIZE, GRID_SIZE);
}

void NeuralGridRenderer::renderSensoryData(const TreeSensoryData& sensory, lv_obj_t* parent)
{
    if (!gridCanvas_) {
        initialize(parent);
    }

    if (!gridCanvas_) {
        spdlog::error("NeuralGridRenderer::renderSensoryData: Canvas not initialized");
        return;
    }

    // Get drawing layer.
    lv_layer_t layer;
    lv_canvas_init_layer(gridCanvas_, &layer);

    // Render each neural grid cell.
    for (uint32_t ny = 0; ny < GRID_SIZE; ny++) {
        for (uint32_t nx = 0; nx < GRID_SIZE; nx++) {
            const auto& histogram = sensory.material_histograms[ny][nx];

            // Convert histogram to color.
            lv_color_t cellColor = histogramToColor(histogram);

            // Calculate purity for opacity.
            double purity = calculatePurity(histogram);
            lv_opa_t opacity = static_cast<lv_opa_t>(purity * 255);

            // Draw filled rectangle for this cell.
            lv_area_t area;
            area.x1 = nx * CELL_SIZE;
            area.y1 = ny * CELL_SIZE;
            area.x2 = area.x1 + CELL_SIZE - 1;
            area.y2 = area.y1 + CELL_SIZE - 1;

            lv_draw_rect_dsc_t rect_dsc;
            lv_draw_rect_dsc_init(&rect_dsc);
            rect_dsc.bg_color = cellColor;
            rect_dsc.bg_opa = opacity;
            rect_dsc.border_width = 1;
            rect_dsc.border_color = lv_color_hex(0x404040);
            rect_dsc.border_opa = LV_OPA_50;

            lv_draw_rect(&layer, &rect_dsc, &area);
        }
    }

    lv_canvas_finish_layer(gridCanvas_, &layer);

    if (!thoughtLabel_) {
        thoughtLabel_ = lv_label_create(parent);
        lv_obj_set_style_text_color(thoughtLabel_, lv_color_hex(0xFFFFFF), 0);
        lv_obj_align(thoughtLabel_, LV_ALIGN_BOTTOM_MID, 0, -10);
    }

    if (!energyLabel_) {
        energyLabel_ = lv_label_create(parent);
        lv_obj_set_style_text_color(energyLabel_, lv_color_hex(0xFFD700), 0);
        lv_obj_align(energyLabel_, LV_ALIGN_TOP_MID, 0, 10);
    }

    lv_label_set_text(thoughtLabel_, sensory.current_thought.c_str());

    char energy_text[64];
    snprintf(energy_text, sizeof(energy_text), "Energy: %.1f", sensory.total_energy);
    lv_label_set_text(energyLabel_, energy_text);
}

void NeuralGridRenderer::renderEmpty(lv_obj_t* parent)
{
    if (!gridCanvas_) {
        initialize(parent);
    }

    if (!gridCanvas_) {
        return;
    }

    // Clear canvas to dark background.
    lv_canvas_fill_bg(gridCanvas_, lv_color_hex(0x202020), LV_OPA_COVER);

    // Get drawing layer for text.
    lv_layer_t layer;
    lv_canvas_init_layer(gridCanvas_, &layer);

    // Draw "No Tree Selected" message.
    lv_draw_label_dsc_t label_dsc;
    lv_draw_label_dsc_init(&label_dsc);
    label_dsc.color = lv_color_hex(0x808080);
    label_dsc.text = "No Tree Selected";

    lv_area_t label_area;
    label_area.x1 = CANVAS_WIDTH / 2 - 60;
    label_area.y1 = CANVAS_HEIGHT / 2 - 10;
    label_area.x2 = CANVAS_WIDTH / 2 + 60;
    label_area.y2 = CANVAS_HEIGHT / 2 + 10;

    lv_draw_label(&layer, &label_dsc, &label_area);

    lv_canvas_finish_layer(gridCanvas_, &layer);
}

void NeuralGridRenderer::cleanup()
{
    if (gridCanvas_) {
        lv_obj_del(gridCanvas_);
        gridCanvas_ = nullptr;
    }

    if (thoughtLabel_) {
        lv_obj_del(thoughtLabel_);
        thoughtLabel_ = nullptr;
    }

    if (energyLabel_) {
        lv_obj_del(energyLabel_);
        energyLabel_ = nullptr;
    }

    canvasBuffer_.clear();
}

lv_color_t NeuralGridRenderer::histogramToColor(
    const std::array<double, TreeSensoryData::NUM_MATERIALS>& histogram) const
{
    // Blend material colors weighted by histogram probabilities.
    double r = 0.0, g = 0.0, b = 0.0;

    for (int i = 0; i < TreeSensoryData::NUM_MATERIALS; i++) {
        if (histogram[i] > 0.0) {
            lv_color_t matColor = getMaterialColor(i);
            uint32_t color32 = lv_color_to_u32(matColor);
            r += histogram[i] * ((color32 >> 16) & 0xFF);
            g += histogram[i] * ((color32 >> 8) & 0xFF);
            b += histogram[i] * (color32 & 0xFF);
        }
    }

    // Clamp and convert to lv_color_t.
    uint8_t rByte = static_cast<uint8_t>(std::min(255.0, r));
    uint8_t gByte = static_cast<uint8_t>(std::min(255.0, g));
    uint8_t bByte = static_cast<uint8_t>(std::min(255.0, b));

    return lv_color_make(rByte, gByte, bByte);
}

double NeuralGridRenderer::calculatePurity(
    const std::array<double, TreeSensoryData::NUM_MATERIALS>& histogram) const
{
    // Purity = max probability (simple approach).
    // For one-hot distributions: purity = 1.0.
    // For uniform distributions: purity = 1/9 ≈ 0.11.
    double maxProb = 0.0;
    for (double prob : histogram) {
        maxProb = std::max(maxProb, prob);
    }

    return maxProb;
}

lv_color_t NeuralGridRenderer::getMaterialColor(int materialIndex) const
{
    // Map material index to MaterialType (alphabetical order).
    // AIR=0, DIRT=1, LEAF=2, METAL=3, SAND=4, SEED=5, WALL=6, WATER=7, WOOD=8.
    static const MaterialType materials[] = { MaterialType::AIR,  MaterialType::DIRT,
                                              MaterialType::LEAF, MaterialType::METAL,
                                              MaterialType::SAND, MaterialType::SEED,
                                              MaterialType::WALL, MaterialType::WATER,
                                              MaterialType::WOOD };

    if (materialIndex < 0 || materialIndex >= TreeSensoryData::NUM_MATERIALS) {
        return lv_color_hex(0x000000); // Black for invalid index.
    }

    MaterialType type = materials[materialIndex];

    // Material color mapping (matching CellRenderer).
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

} // namespace Ui
} // namespace DirtSim
