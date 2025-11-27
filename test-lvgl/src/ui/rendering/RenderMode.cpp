#include "RenderMode.h"
#include <nlohmann/json.hpp>
#include <stdexcept>

namespace DirtSim {
namespace Ui {

std::string renderModeToString(RenderMode mode)
{
    switch (mode) {
        case RenderMode::SHARP:
            return "sharp";
        case RenderMode::SMOOTH:
            return "smooth";
        case RenderMode::PIXEL_PERFECT:
            return "pixel_perfect";
        case RenderMode::LVGL_DEBUG:
            return "lvgl_debug";
        case RenderMode::ADAPTIVE:
            return "adaptive";
        default:
            return "sharp";
    }
}

RenderMode stringToRenderMode(const std::string& str)
{
    if (str == "sharp") return RenderMode::SHARP;
    if (str == "smooth") return RenderMode::SMOOTH;
    if (str == "pixel_perfect") return RenderMode::PIXEL_PERFECT;
    if (str == "lvgl_debug") return RenderMode::LVGL_DEBUG;
    if (str == "adaptive") return RenderMode::ADAPTIVE;
    return RenderMode::SHARP; // Default fallback.
}

void to_json(nlohmann::json& j, RenderMode mode)
{
    j = renderModeToString(mode);
}

void from_json(const nlohmann::json& j, RenderMode& mode)
{
    if (!j.is_string()) {
        throw std::runtime_error("RenderMode::from_json: JSON value must be a string");
    }
    mode = stringToRenderMode(j.get<std::string>());
}

} // namespace Ui
} // namespace DirtSim
