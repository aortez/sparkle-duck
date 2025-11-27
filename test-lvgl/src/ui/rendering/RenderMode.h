#pragma once

#include <nlohmann/json_fwd.hpp>
#include <string>

namespace DirtSim {
namespace Ui {

/**
 * @brief Rendering mode for the world grid.
 *
 * Different modes optimize for different grid sizes and use cases.
 */
enum class RenderMode {
    // Pixel renderer without filtering - best for large cells.
    SHARP,

    // Pixel renderer with bilinear filtering - best for dense grids (>200x200).
    SMOOTH,

    // Integer-only scaling (2×, 3×, etc.) - perfectly crisp pixels, no interpolation.
    PIXEL_PERFECT,

    // Full LVGL renderer with debug visualization (COM, vectors, pressure).
    LVGL_DEBUG,

    // Automatically choose based on cell size.
    // Uses SMOOTH for cells <4px, SHARP otherwise.
    ADAPTIVE
};

/**
 * @brief Convert RenderMode to string for serialization.
 */
std::string renderModeToString(RenderMode mode);

/**
 * @brief Convert string to RenderMode for deserialization.
 */
RenderMode stringToRenderMode(const std::string& str);

/**
 * @brief JSON serialization for RenderMode.
 */
void to_json(nlohmann::json& j, RenderMode mode);

/**
 * @brief JSON deserialization for RenderMode.
 */
void from_json(const nlohmann::json& j, RenderMode& mode);

} // namespace Ui
} // namespace DirtSim
