#pragma once

#include <cstdint>
#include <lvgl/lvgl.h>
#include <vector>

namespace DirtSim {
namespace Ui {

/**
 * @brief Julia set fractal renderer with palette cycling animation.
 * Renders to an LVGL canvas filling the entire screen as a background.
 */
class JuliaFractal {
public:
    /**
     * @brief Create Julia fractal renderer.
     * @param parent Parent LVGL object to attach the canvas to.
     * @param windowWidth Full window width.
     * @param windowHeight Full window height.
     */
    JuliaFractal(lv_obj_t* parent, int windowWidth, int windowHeight);
    ~JuliaFractal();

    /**
     * @brief Update animation (palette cycle).
     * Call this each frame to animate the colors.
     */
    void update();

    /**
     * @brief Resize the fractal to match new window dimensions.
     * @param newWidth New window width.
     * @param newHeight New window height.
     */
    void resize(int newWidth, int newHeight);

    /**
     * @brief Get the canvas object.
     */
    lv_obj_t* getCanvas() const { return canvas_; }

private:
    /**
     * @brief Calculate Julia set iteration count for a point.
     * @param x Pixel x coordinate.
     * @param y Pixel y coordinate.
     * @return Iteration count (0 to maxIterations_).
     */
    int calculateJuliaPoint(int x, int y) const;

    /**
     * @brief Render the fractal to the canvas buffer.
     */
    void render();

    /**
     * @brief Update only colors (fast palette cycling without recalculating fractal).
     */
    void updateColors();

    /**
     * @brief Get color from palette with cycling offset.
     * @param iteration Iteration count from Julia calculation.
     * @return ARGB color value.
     */
    uint32_t getPaletteColor(int iteration) const;

    lv_obj_t* canvas_ = nullptr;
    lv_color_t* canvasBuffer_ = nullptr;
    int width_ = 0;
    int height_ = 0;
    double paletteOffset_ = 0.0; // Floating point for smooth sinusoidal speed.
    int maxIterations_ = 128;

    double animationPhase_ = 0.0; // Phase for sine wave (0 to 2π).

    // Cached iteration counts to avoid recalculating fractal every frame.
    std::vector<int> iterationCache_;

    // Julia set parameters (c = cReal + cImag*i).
    double cReal_ = -0.7;
    double cImag_ = 0.27;

    // View bounds in complex plane.
    double xMin_ = -1.5;
    double xMax_ = 1.5;
    double yMin_ = -1.5;
    double yMax_ = 1.5;
};

} // namespace Ui
} // namespace DirtSim
