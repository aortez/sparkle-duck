#pragma once

#include "core/organisms/TreeSensoryData.h"
#include "lvgl/lvgl.h"
#include <cstdint>
#include <optional>
#include <vector>

namespace DirtSim {
namespace Ui {

class NeuralGridRenderer {
public:
    NeuralGridRenderer() = default;
    ~NeuralGridRenderer();

    /**
     * @brief Initialize the neural grid renderer.
     * @param parent LVGL parent container for the canvas.
     */
    void initialize(lv_obj_t* parent);

    /**
     * @brief Render tree sensory data to the neural grid.
     * @param sensory Tree's sensory data (15x15 material histograms).
     */
    void renderSensoryData(const TreeSensoryData& sensory, lv_obj_t* parent);

    /**
     * @brief Render empty state (no tree selected).
     */
    void renderEmpty(lv_obj_t* parent);

    /**
     * @brief Clean up resources.
     */
    void cleanup();

private:
    lv_obj_t* gridCanvas_ = nullptr;
    lv_obj_t* thoughtLabel_ = nullptr;
    lv_obj_t* energyLabel_ = nullptr;
    std::vector<uint8_t> canvasBuffer_;

    static constexpr uint32_t GRID_SIZE = 15;
    static constexpr uint32_t CELL_SIZE = 32;
    static constexpr uint32_t CANVAS_WIDTH = GRID_SIZE * CELL_SIZE;
    static constexpr uint32_t CANVAS_HEIGHT = GRID_SIZE * CELL_SIZE;

    /**
     * @brief Convert material histogram to RGB color.
     * @param histogram Array of 9 material probabilities.
     * @return Blended color representing material distribution.
     */
    lv_color_t histogramToColor(
        const std::array<double, TreeSensoryData::NUM_MATERIALS>& histogram) const;

    /**
     * @brief Calculate histogram purity (how one-hot it is).
     * @param histogram Array of material probabilities.
     * @return Purity value [0.0, 1.0] where 1.0 is one-hot, 0.0 is uniform.
     */
    double calculatePurity(
        const std::array<double, TreeSensoryData::NUM_MATERIALS>& histogram) const;

    /**
     * @brief Get material color by type.
     */
    lv_color_t getMaterialColor(int materialIndex) const;
};

} // namespace Ui
} // namespace DirtSim
