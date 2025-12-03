#include "DisplayCapture.h"
#include <cstring>
#include <lvgl/lvgl.h>
#include <spdlog/spdlog.h>

// C API for lodepng.
extern "C" {
unsigned lodepng_encode32_file(
    const char* filename, const unsigned char* image, unsigned w, unsigned h);
const char* lodepng_error_text(unsigned code);
}

namespace DirtSim {
namespace Ui {

std::optional<ScreenshotData> captureDisplayPixels(_lv_display_t* display)
{
    if (!display) {
        spdlog::error("DisplayCapture: Display is null");
        return std::nullopt;
    }

    // Get display dimensions.
    uint32_t width = lv_display_get_horizontal_resolution(display);
    uint32_t height = lv_display_get_vertical_resolution(display);

    if (width == 0 || height == 0) {
        spdlog::error("DisplayCapture: Display has zero dimensions");
        return std::nullopt;
    }

    // Get the screen (root object of the display).
    lv_obj_t* screen = lv_display_get_screen_active(display);
    if (!screen) {
        spdlog::error("DisplayCapture: No active screen on display");
        return std::nullopt;
    }

    // Take snapshot using LVGL's snapshot API.
    lv_draw_buf_t* drawBuf = lv_snapshot_take(screen, LV_COLOR_FORMAT_ARGB8888);
    if (!drawBuf) {
        spdlog::error("DisplayCapture: lv_snapshot_take failed");
        return std::nullopt;
    }

    // Get buffer info.
    uint32_t bufWidth = drawBuf->header.w;
    uint32_t bufHeight = drawBuf->header.h;
    const uint8_t* bufData = static_cast<const uint8_t*>(drawBuf->data);
    size_t bufSize = static_cast<size_t>(bufWidth) * bufHeight * 4; // ARGB8888 = 4 bytes/pixel

    // Copy pixel data.
    ScreenshotData data;
    data.width = bufWidth;
    data.height = bufHeight;
    data.pixels.resize(bufSize);
    std::memcpy(data.pixels.data(), bufData, bufSize);

    // Free the draw buffer.
    lv_draw_buf_destroy(drawBuf);

    spdlog::info("DisplayCapture: Captured {}x{} ({} bytes)", bufWidth, bufHeight, bufSize);
    return data;
}

bool savePNG(const uint8_t* pixels, uint32_t width, uint32_t height, const std::string& filepath)
{
    // Convert ARGB8888 to RGBA8888 for lodepng.
    size_t pixelCount = static_cast<size_t>(width) * height;
    std::vector<uint8_t> rgbaPixels(pixelCount * 4);

    for (size_t i = 0; i < pixelCount * 4; i += 4) {
        // LVGL ARGB8888 is little-endian: B G R A in memory.
        // lodepng wants RGBA: R G B A.
        rgbaPixels[i + 0] = pixels[i + 2]; // R (from B position)
        rgbaPixels[i + 1] = pixels[i + 1]; // G (same position)
        rgbaPixels[i + 2] = pixels[i + 0]; // B (from R position)
        rgbaPixels[i + 3] = pixels[i + 3]; // A (same position)
    }

    unsigned error = lodepng_encode32_file(filepath.c_str(), rgbaPixels.data(), width, height);

    if (error) {
        spdlog::error(
            "DisplayCapture: PNG encoding failed: {} ({})", lodepng_error_text(error), error);
        return false;
    }

    spdlog::info("DisplayCapture: Saved PNG to {}", filepath);
    return true;
}

} // namespace Ui
} // namespace DirtSim
