#pragma once

#include "core/Cell.h"
#include "core/WorldData.h"
#include "lvgl/lvgl.h"
#include <cstdint>
#include <vector>

namespace DirtSim {
namespace Ui {

class CellRenderer {
public:
    CellRenderer() = default;
    ~CellRenderer();

    void initialize(lv_obj_t* parent, uint32_t worldWidth, uint32_t worldHeight);
    void resize(lv_obj_t* parent, uint32_t worldWidth, uint32_t worldHeight);
    void renderWorldData(
        const WorldData& worldData,
        lv_obj_t* parent,
        bool debugDraw,
        bool usePixelRenderer = false);
    void cleanup();

private:
    // Single canvas for entire world grid
    lv_obj_t* worldCanvas_ = nullptr;
    std::vector<uint8_t> canvasBuffer_;

    // Canvas dimensions (fixed size)
    uint32_t canvasWidth_ = 0;
    uint32_t canvasHeight_ = 0;

    // World dimensions (variable)
    uint32_t width_ = 0;
    uint32_t height_ = 0;
    lv_obj_t* parent_ = nullptr;

    // Track container size for resize detection.
    int32_t lastContainerWidth_ = 0;
    int32_t lastContainerHeight_ = 0;

    // Scaled cell dimensions for fitting the drawing area
    uint32_t scaledCellWidth_ = Cell::WIDTH;
    uint32_t scaledCellHeight_ = Cell::HEIGHT;
    double scaleX_ = 1.0;
    double scaleY_ = 1.0;

    void calculateScaling(uint32_t worldWidth, uint32_t worldHeight);

    // Direct rendering to single canvas at scaled resolution (optimized)
    void renderCellDirectOptimized(
        const Cell& cell,
        lv_layer_t& layer,
        int32_t cellX,
        int32_t cellY,
        bool debugDraw,
        bool usePixelRenderer);
};

} // namespace Ui
} // namespace DirtSim
