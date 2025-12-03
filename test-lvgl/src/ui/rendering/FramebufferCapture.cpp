#include "FramebufferCapture.h"

#include <lvgl.h>
#include <spdlog/spdlog.h>

// TODO: Download stb_image_write.h to external/stb/
// For now, we'll use a placeholder implementation
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "external/stb/stb_image_write.h"

namespace DirtSim {
namespace Ui {

namespace {

void lvglPixelsToRGBA(const lv_color_t* lv_pixels, uint8_t* rgba_pixels, int width, int height)
{
    for (int i = 0; i < width * height; i++) {
        rgba_pixels[i * 4 + 0] = lv_pixels[i].red;
        rgba_pixels[i * 4 + 1] = lv_pixels[i].green;
        rgba_pixels[i * 4 + 2] = lv_pixels[i].blue;
        rgba_pixels[i * 4 + 3] = 255;
    }
}

} // namespace

std::unique_ptr<CapturedFrame> FramebufferCapture::capture(
    lv_display_t* display, ImageFormat format)
{
    if (!display) {
        spdlog::error("FramebufferCapture: display is null");
        return nullptr;
    }

    int32_t width = lv_display_get_horizontal_resolution(display);
    int32_t height = lv_display_get_vertical_resolution(display);

    lv_draw_buf_t* draw_buf = lv_display_get_buf_active(display);
    if (!draw_buf || !draw_buf->data) {
        spdlog::error("FramebufferCapture: no active draw buffer");
        return nullptr;
    }

    const uint8_t* buffer_data = draw_buf->data;
    lv_color_format_t color_format = static_cast<lv_color_format_t>(draw_buf->header.cf);

    std::vector<uint8_t> rgba_pixels(width * height * 4);

    if (color_format == LV_COLOR_FORMAT_RGB888 || color_format == LV_COLOR_FORMAT_XRGB8888
        || color_format == LV_COLOR_FORMAT_ARGB8888) {
        int bytes_per_pixel = (color_format == LV_COLOR_FORMAT_RGB888) ? 3 : 4;
        for (int i = 0; i < width * height; i++) {
            rgba_pixels[i * 4 + 0] = buffer_data[i * bytes_per_pixel + 0];
            rgba_pixels[i * 4 + 1] = buffer_data[i * bytes_per_pixel + 1];
            rgba_pixels[i * 4 + 2] = buffer_data[i * bytes_per_pixel + 2];
            rgba_pixels[i * 4 + 3] = 255;
        }
    }
    else {
        const lv_color_t* lv_pixels = reinterpret_cast<const lv_color_t*>(buffer_data);
        lvglPixelsToRGBA(lv_pixels, rgba_pixels.data(), width, height);
    }

    auto frame = std::make_unique<CapturedFrame>();
    frame->width = width;
    frame->height = height;
    frame->format = format;

    if (format == ImageFormat::PNG) {
        frame->data = encodePNG(rgba_pixels.data(), width, height);
    }
    else {
        frame->data = encodeJPEG(rgba_pixels.data(), width, height, 85);
    }

    if (frame->data.empty()) {
        spdlog::error("FramebufferCapture: encoding failed");
        return nullptr;
    }

    spdlog::debug(
        "FramebufferCapture: captured {}x{} frame ({} bytes)", width, height, frame->data.size());

    return frame;
}

std::vector<uint8_t> FramebufferCapture::encodePNG(const uint8_t* pixels, int width, int height)
{
    std::vector<uint8_t> result;

    auto write_func = [](void* context, void* data, int size) {
        auto* vec = static_cast<std::vector<uint8_t>*>(context);
        const uint8_t* bytes = static_cast<const uint8_t*>(data);
        vec->insert(vec->end(), bytes, bytes + size);
    };

    if (stbi_write_png_to_func(write_func, &result, width, height, 4, pixels, width * 4) == 0) {
        spdlog::error("FramebufferCapture: PNG encoding failed");
        return {};
    }

    return result;
}

std::vector<uint8_t> FramebufferCapture::encodeJPEG(
    const uint8_t* pixels, int width, int height, int quality)
{
    std::vector<uint8_t> result;

    auto write_func = [](void* context, void* data, int size) {
        auto* vec = static_cast<std::vector<uint8_t>*>(context);
        const uint8_t* bytes = static_cast<const uint8_t*>(data);
        vec->insert(vec->end(), bytes, bytes + size);
    };

    if (stbi_write_jpg_to_func(write_func, &result, width, height, 4, pixels, quality) == 0) {
        spdlog::error("FramebufferCapture: JPEG encoding failed");
        return {};
    }

    return result;
}

} // namespace Ui
} // namespace DirtSim
