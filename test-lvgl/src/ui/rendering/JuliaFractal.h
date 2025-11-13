#pragma once

#include <atomic>
#include <cstdint>
#include <lvgl/lvgl.h>
#include <mutex>
#include <thread>
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
     * @param cReal Julia constant real part.
     * @param cImag Julia constant imaginary part.
     * @param maxIter Maximum iterations.
     * @return Iteration count (0 to maxIter).
     */
    int calculateJuliaPoint(int x, int y, double cReal, double cImag, int maxIter) const;

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
    int lastRenderedMaxIterations_ = 128; // Track when to recalculate fractal.

    // Sinusoidal animation phases.
    double animationPhase_ = 0.0; // Phase for palette cycling (0 to 2π).
    double detailPhase_ = 0.0;    // Phase for detail oscillation (0 to 2π).
    double cPhase_ = 0.0;         // Phase for Julia constant morphing (0 to 2π).

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

    // Background rendering thread for smooth animation.
    std::thread renderThread_;
    std::atomic<bool> shouldExit_{ false };
    std::mutex bufferMutex_;

    // Triple buffering - 3 buffers rotate through roles.
    lv_color_t* buffers_[3] = { nullptr, nullptr, nullptr };
    std::vector<int> iterationCaches_[3];

    // Buffer indices (which buffer has which role).
    std::atomic<int> frontBufferIdx_{ 0 }; // Currently displaying.
    std::atomic<int> readyBufferIdx_{ 1 }; // Ready to swap.
    int renderBufferIdx_ = 2;              // Being rendered (only accessed by render thread).

    std::atomic<bool> readyBufferAvailable_{ false };

    // Background thread render function.
    void renderThreadFunc();
};

} // namespace Ui
} // namespace DirtSim
