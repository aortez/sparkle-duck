#include "DisplayCapture.h"
#include <array>
#include <cstring>
#include <lvgl/lvgl.h>
#include <spdlog/spdlog.h>

// C API for lodepng.
extern "C" {
unsigned lodepng_encode32(
    unsigned char** out, size_t* outsize, const unsigned char* image, unsigned w, unsigned h);
const char* lodepng_error_text(unsigned code);
}

namespace DirtSim {
namespace Ui {

std::optional<ScreenshotData> captureDisplayPixels(_lv_display_t* display, double /*scale*/)
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

std::vector<uint8_t> encodePNG(const uint8_t* pixels, uint32_t width, uint32_t height)
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

    unsigned char* pngData = nullptr;
    size_t pngSize = 0;
    unsigned error = lodepng_encode32(&pngData, &pngSize, rgbaPixels.data(), width, height);

    if (error) {
        spdlog::error(
            "DisplayCapture: PNG encoding failed: {} ({})", lodepng_error_text(error), error);
        return {};
    }

    // Copy to vector and free lodepng memory.
    std::vector<uint8_t> result(pngData, pngData + pngSize);
    free(pngData);

    spdlog::info("DisplayCapture: Encoded PNG ({} bytes)", pngSize);
    return result;
}

std::string base64Encode(const std::vector<uint8_t>& data)
{
    static const char base64_chars[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    std::string encoded;
    encoded.reserve((data.size() + 2) / 3 * 4);

    size_t i = 0;
    uint8_t char_array_3[3];
    uint8_t char_array_4[4];

    for (size_t idx = 0; idx < data.size(); idx++) {
        char_array_3[i++] = data[idx];
        if (i == 3) {
            char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
            char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
            char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
            char_array_4[3] = char_array_3[2] & 0x3f;

            for (i = 0; i < 4; i++)
                encoded += base64_chars[char_array_4[i]];
            i = 0;
        }
    }

    if (i) {
        for (size_t j = i; j < 3; j++)
            char_array_3[j] = '\0';

        char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
        char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
        char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);

        for (size_t j = 0; j < i + 1; j++)
            encoded += base64_chars[char_array_4[j]];

        while (i++ < 3)
            encoded += '=';
    }

    return encoded;
}

} // namespace Ui
} // namespace DirtSim
