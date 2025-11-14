#pragma once

#include "core/organisms/TreeTypes.h"
#include "lvgl/lvgl.h"
#include <cstdint>
#include <optional>
#include <vector>

namespace DirtSim {
namespace Ui {

/**
 * @brief Renders the 15x15 neural grid visualization for tree organisms.
 *
 * Shows how a tree "sees" the world through its scale-invariant sensory system.
 * Each cell displays material histograms as color-coded blocks with opacity
 * indicating histogram purity (one-hot vs mixed).
 */
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
    // Canvas for rendering the 15x15 grid.
    lv_obj_t* gridCanvas_ = nullptr;
    std::vector<uint8_t> canvasBuffer_;

    // Canvas dimensions.
    static constexpr uint32_t GRID_SIZE = 15;
    static constexpr uint32_t CELL_SIZE = 32; // Pixels per neural cell.
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
