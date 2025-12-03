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
 * @param scale Resolution scale factor (0.25 = 4x smaller, 1.0 = full res).
 * @return Pixel data in ARGB8888 format, or std::nullopt if capture failed.
 */
std::optional<ScreenshotData> captureDisplayPixels(_lv_display_t* display, double scale = 1.0);

/**
 * @brief Encode ARGB8888 pixel data to PNG bytes.
 * @param pixels ARGB8888 pixel data.
 * @param width Image width.
 * @param height Image height.
 * @return PNG-encoded bytes, or empty vector on failure.
 */
std::vector<uint8_t> encodePNG(const uint8_t* pixels, uint32_t width, uint32_t height);

/**
 * @brief Encode binary data to base64 string.
 * @param data Binary data to encode.
 * @return Base64-encoded string.
 */
std::string base64Encode(const std::vector<uint8_t>& data);

} // namespace Ui
} // namespace DirtSim
