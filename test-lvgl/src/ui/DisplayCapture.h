#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

// Forward declaration for LVGL display structure.
struct _lv_display_t;

namespace DirtSim {
namespace Ui {

/**
 * @brief Screenshot pixel data (ARGB8888 format).
 */
struct ScreenshotData {
    std::vector<uint8_t> pixels; // ARGB8888 pixel data.
    uint32_t width;
    uint32_t height;
};

/**
 * @brief Capture entire LVGL display as raw pixel data.
 * @param display LVGL display to capture.
 * @return Pixel data in ARGB8888 format, or std::nullopt if capture failed.
 */
std::optional<ScreenshotData> captureDisplayPixels(_lv_display_t* display);

/**
 * @brief Encode ARGB8888 pixel data to PNG file.
 * @param pixels ARGB8888 pixel data.
 * @param width Image width.
 * @param height Image height.
 * @param filepath Output PNG file path.
 * @return true if successful, false otherwise.
 */
bool savePNG(const uint8_t* pixels, uint32_t width, uint32_t height, const std::string& filepath);

} // namespace Ui
} // namespace DirtSim
