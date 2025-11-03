#pragma once

#include "../../core/Cell.h"
#include "../../core/WorldData.h"
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
    void renderWorldData(const WorldData& worldData, lv_obj_t* parent, bool debugDraw);
    void cleanup();

private:
    struct CellCanvas {
        lv_obj_t* canvas = nullptr;
        std::vector<uint8_t> buffer;
    };

    std::vector<std::vector<CellCanvas>> canvases_;
    uint32_t width_ = 0;
    uint32_t height_ = 0;
    lv_obj_t* parent_ = nullptr;

    void renderCell(Cell& cell, uint32_t x, uint32_t y, bool debugDraw);
    void renderCellNormal(Cell& cell, CellCanvas& canvas, uint32_t x, uint32_t y);
    void renderCellDebug(Cell& cell, CellCanvas& canvas, uint32_t x, uint32_t y);
};

} // namespace Ui
} // namespace DirtSim
