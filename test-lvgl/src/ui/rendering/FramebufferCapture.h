#pragma once

#include <cstdint>
#include <memory>
#include <vector>

typedef struct _lv_display_t lv_display_t;

namespace DirtSim {
namespace Ui {

enum class ImageFormat { PNG, JPEG };

struct CapturedFrame {
    std::vector<uint8_t> data;
    ImageFormat format;
    int width;
    int height;
};

class FramebufferCapture {
public:
    static std::unique_ptr<CapturedFrame> capture(lv_display_t* display, ImageFormat format);

private:
    static std::vector<uint8_t> encodePNG(const uint8_t* pixels, int width, int height);
    static std::vector<uint8_t> encodeJPEG(
        const uint8_t* pixels, int width, int height, int quality);
};

} // namespace Ui
} // namespace DirtSim
