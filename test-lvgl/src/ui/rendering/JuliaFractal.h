#pragma once

#include <atomic>
#include <cstdint>
#include <lvgl/lvgl.h>
#include <mutex>
#include <random>
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

    /**
     * @brief Advance to next fractal parameters immediately.
     * Triggers a new random parameter set with smooth transition.
     */
    void advanceToNextFractal();

    /**
     * @brief Get current Julia constant real part.
     * Thread-safe access to fractal parameter.
     */
    double getCReal() const;

    /**
     * @brief Get current Julia constant imaginary part.
     * Thread-safe access to fractal parameter.
     */
    double getCImag() const;

    /**
     * @brief Get current region name.
     * Returns the human-readable name of the current region, or "Random Exploration".
     */
    const char* getRegionName() const;

    /**
     * @brief Get minimum iterations (detail oscillation lower bound).
     */
    int getMinIterations() const { return minIterationBound_; }

    /**
     * @brief Get maximum iterations (detail oscillation upper bound).
     */
    int getMaxIterations() const { return maxIterationBound_; }

    /**
     * @brief Get current iteration count (oscillates between min and max).
     */
    int getCurrentIterations() const;

    /**
     * @brief Get current transitioning minimum iterations (accounts for smooth transitions).
     */
    int getTransitioningMinIterations() const;

    /**
     * @brief Get current transitioning maximum iterations (accounts for smooth transitions).
     */
    int getTransitioningMaxIterations() const;

    /**
     * @brief Get all iteration info atomically (min, current, max).
     * Thread-safe - reads all three values under a single lock to prevent inconsistencies.
     * @param outMin Transitioning minimum iterations.
     * @param outCurrent Current iteration count.
     * @param outMax Transitioning maximum iterations.
     */
    void getIterationInfo(int& outMin, int& outCurrent, int& outMax) const;

    /**
     * @brief Get current display FPS (update() call frequency).
     */
    double getDisplayFps() const;

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

    // Dynamic parameter system for randomized exploration.
    std::mt19937 rng_;
    double changeTimer_ = 0.0;
    double currentChangeInterval_ = 40.0; // Seconds between parameter changes.
    double lastUpdateTime_ = 0.0;         // For delta time calculation.

    // Dynamic resolution scaling based on FPS.
    double currentResolutionDivisor_ = 2.0; // Current resolution divisor (smooth adaptive).
    int baseWindowWidth_ = 0;               // Original window dimensions.
    int baseWindowHeight_ = 0;
    double fpsSum_ = 0.0;                // Rolling sum for FPS calculation (render thread).
    int fpsSampleCount_ = 0;             // Number of samples in rolling window.
    double lastFpsCheckTime_ = 0.0;      // Time of last FPS check.
    double lastFpsLogTime_ = 0.0;        // Time of last INFO level FPS log.
    double displayFpsSum_ = 0.0;         // Rolling sum for display FPS (update() calls).
    int displayFpsSampleCount_ = 0;      // Number of display samples.
    double lastDisplayUpdateTime_ = 0.0; // Time of last update() call.
    constexpr static double FPS_CHECK_INTERVAL = 2.0;
    constexpr static double FPS_LOG_INTERVAL = 10.0;
    constexpr static int FPS_SAMPLE_COUNT = 60; // Average over 60 frames.
    constexpr static double TARGET_FPS = 60.0;

    // Current animation parameters (instance variables, not constants).
    double phaseSpeed_ = 0.0;
    double detailPhaseSpeed_ = 0.01;
    double cPhaseSpeed_ = 0.01;
    double cRealCenter_ = -0.7;
    double cRealAmplitude_ = 0.1;
    double cImagCenter_ = 0.27;
    double cImagAmplitude_ = 0.1;
    int minIterationBound_ = 0;   // Lower bound for detail oscillation (not overwritten).
    int maxIterationBound_ = 200; // Upper bound for detail oscillation (not overwritten).
    int maxIterations_ = 200;     // Current max iterations used for rendering (oscillates).

    // Smooth transition between parameter sets.
    double transitionProgress_ = 1.0; // 0.0 to 1.0 (1.0 = no transition).
    double transitionDuration_ = 5.0; // Seconds for smooth transition.
    double oldCRealCenter_ = -0.7;
    double oldCRealAmplitude_ = 0.1;
    double oldCImagCenter_ = 0.27;
    double oldCImagAmplitude_ = 0.1;
    double oldDetailPhaseSpeed_ = 0.01;
    double oldCPhaseSpeed_ = 0.01;
    int oldMinIterationBound_ = 0;
    int oldMaxIterationBound_ = 200;

    // Interesting Julia set regions (curated presets).
    static constexpr int NUM_REGIONS = 10;
    static constexpr std::pair<double, double> INTERESTING_REGIONS[NUM_REGIONS] = {
        { -0.7, 0.27 },        // Douady's Rabbit (classic).
        { -0.4, 0.6 },         // Dendrite (branching tree).
        { -0.8, 0.156 },       // Spiral arms.
        { -0.835, -0.2321 },   // Complex spirals.
        { -0.74543, 0.11301 }, // Delicate branches.
        { 0.285, 0.01 },       // Siegel disk (near-circular).
        { -0.123, 0.745 },     // Dragon-like curves.
        { 0.3, 0.5 },          // Swirling patterns.
        { -1.0, 0.0 },         // Period-2 bulb.
        { -0.12, 0.75 },       // Upper region variations.
    };

    // Human-readable names for the curated regions.
    static constexpr const char* REGION_NAMES[NUM_REGIONS] = {
        "Douady's Rabbit",         "Dendrite",          "Spiral Arms",
        "Complex Spirals",         "Delicate Branches", "Siegel Disk",
        "Dragon-like Curves",      "Swirling Patterns", "Period-2 Bulb",
        "Upper Region Variations",
    };

    int currentRegionIdx_ = -1; // Current region index (-1 = random exploration).

    void generateRandomParameters(); // Generate new random parameter set.

    // Background rendering thread for smooth animation.
    std::thread renderThread_;
    std::atomic<bool> shouldExit_{ false };
    std::atomic<bool> resizeNeeded_{ false }; // Signal from render thread to main thread.
    std::mutex bufferMutex_;
    mutable std::mutex parameterMutex_; // Protects animation parameters from race conditions.

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
