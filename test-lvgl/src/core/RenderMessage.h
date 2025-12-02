#pragma once

#include "ReflectSerializer.h"
#include "ScenarioConfig.h"
#include "Vector2.h"
#include "organisms/TreeSensoryData.h"
#include <cstdint>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <vector>
#include <zpp_bits.h>

namespace DirtSim {

/**
 * @brief Render format types for optimized network transmission.
 *
 * Defines the level of detail sent from server to UI clients.
 * Different formats trade payload size for rendering capability.
 */
enum class RenderFormat : uint8_t {
    BASIC = 0, // Minimal: material + fill only (2 bytes/cell = ~45 KB for 150×150).
    DEBUG = 1  // Debug: + COM, velocity, pressure (16 bytes/cell = ~360 KB for 150×150).
};

/**
 * @brief Basic cell data for rendering (2 bytes).
 *
 * Contains only material type and fill ratio - sufficient for basic visualization.
 * Fill ratio is quantized to 8-bit precision (256 levels).
 */
struct BasicCell {
    uint8_t material_type; // MaterialType enum value (0-9).
    uint8_t fill_ratio;    // Quantized [0.0, 1.0] → [0, 255].

    using serialize = zpp::bits::members<2>;
};

/**
 * @brief Debug cell data for physics visualization (24 bytes).
 *
 * Includes material, fill ratio, and quantized physics data for debug overlays.
 * All floating-point values are converted to fixed-point integers.
 */
struct DebugCell {
    uint8_t material_type;        // MaterialType enum value (0-9).
    uint8_t fill_ratio;           // Quantized [0.0, 1.0] → [0, 255].
    uint8_t has_any_support;      // Boolean: cell has structural support.
    uint8_t has_vertical_support; // Boolean: cell has vertical support specifically.

    int16_t com_x;      // Center of mass X: [-1.0, 1.0] → [-32767, 32767].
    int16_t com_y;      // Center of mass Y: [-1.0, 1.0] → [-32767, 32767].
    int16_t velocity_x; // Velocity X: [-10.0, 10.0] → [-32767, 32767].
    int16_t velocity_y; // Velocity Y: [-10.0, 10.0] → [-32767, 32767].

    uint16_t pressure_hydro;   // Hydrostatic pressure: [0, 1000] → [0, 65535].
    uint16_t pressure_dynamic; // Dynamic pressure: [0, 1000] → [0, 65535].

    Vector2<float> pressure_gradient; // Pressure gradient vector.

    using serialize = zpp::bits::members<11>;
};

/**
 * @brief Sparse organism data.
 *
 * Instead of sending organism_id for every cell (mostly zeros), we send a sparse
 * representation: organism ID + list of cells it occupies.
 *
 * Example: 1 tree with 100 cells:
 *   Dense: 22,500 bytes (1 byte per cell)
 *   Sparse: ~200 bytes (1 byte ID + 100 × 2 byte indices)
 */
struct OrganismData {
    uint8_t organism_id;                // Organism identifier (1-255, 0 = none).
    std::vector<uint16_t> cell_indices; // Flat grid indices (y * width + x).

    using serialize = zpp::bits::members<2>;
};

/**
 * @brief Bone connection data for organism structural visualization.
 *
 * Represents spring connections between organism cells.
 * Rendered as lines to show the organism's internal structure.
 */
struct BoneData {
    Vector2i cell_a; // First cell position.
    Vector2i cell_b; // Second cell position.

    using serialize = zpp::bits::members<2>;
};

inline void to_json(nlohmann::json& j, const BoneData& bone)
{
    j = nlohmann::json{ { "cell_a", bone.cell_a }, { "cell_b", bone.cell_b } };
}

inline void from_json(const nlohmann::json& j, BoneData& bone)
{
    bone.cell_a = j.at("cell_a").get<Vector2i>();
    bone.cell_b = j.at("cell_b").get<Vector2i>();
}

/**
 * @brief Render message containing optimized world state.
 *
 * Replaces full WorldData serialization for frame streaming.
 * Format determines payload structure (BasicCell or DebugCell).
 */
struct RenderMessage {
    RenderFormat format; // Which format is payload encoded in?

    // Grid dimensions and simulation state.
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t timestep = 0;
    double fps_server = 0.0;

    // Scenario metadata.
    std::string scenario_id = "empty";
    ScenarioConfig scenario_config = EmptyConfig{};

    // Format-specific cell data (either BasicCell[] or DebugCell[]).
    std::vector<std::byte> payload;

    // Sparse organism tracking (only cells with organism_id != 0).
    std::vector<OrganismData> organisms;

    // Bone connections for structural visualization.
    std::vector<BoneData> bones;

    // Tree organism data (optional - only present when showing a tree's vision).
    std::optional<TreeSensoryData> tree_vision;

    using serialize = zpp::bits::members<11>;
};

/**
 * @brief ADL (Argument-Dependent Lookup) functions for nlohmann::json.
 */
inline void to_json(nlohmann::json& j, const RenderFormat& format)
{
    j = static_cast<uint8_t>(format);
}

inline void from_json(const nlohmann::json& j, RenderFormat& format)
{
    format = static_cast<RenderFormat>(j.get<uint8_t>());
}

inline void to_json(nlohmann::json& j, const BasicCell& cell)
{
    j = nlohmann::json{ { "material_type", cell.material_type },
                        { "fill_ratio", cell.fill_ratio } };
}

inline void from_json(const nlohmann::json& j, BasicCell& cell)
{
    cell.material_type = j.at("material_type").get<uint8_t>();
    cell.fill_ratio = j.at("fill_ratio").get<uint8_t>();
}

inline void to_json(nlohmann::json& j, const DebugCell& cell)
{
    j = nlohmann::json{ { "material_type", cell.material_type },
                        { "fill_ratio", cell.fill_ratio },
                        { "com_x", cell.com_x },
                        { "com_y", cell.com_y },
                        { "velocity_x", cell.velocity_x },
                        { "velocity_y", cell.velocity_y },
                        { "pressure_hydro", cell.pressure_hydro },
                        { "pressure_dynamic", cell.pressure_dynamic } };
}

inline void from_json(const nlohmann::json& j, DebugCell& cell)
{
    cell.material_type = j.at("material_type").get<uint8_t>();
    cell.fill_ratio = j.at("fill_ratio").get<uint8_t>();
    cell.com_x = j.at("com_x").get<int16_t>();
    cell.com_y = j.at("com_y").get<int16_t>();
    cell.velocity_x = j.at("velocity_x").get<int16_t>();
    cell.velocity_y = j.at("velocity_y").get<int16_t>();
    cell.pressure_hydro = j.at("pressure_hydro").get<uint16_t>();
    cell.pressure_dynamic = j.at("pressure_dynamic").get<uint16_t>();
}

inline void to_json(nlohmann::json& j, const OrganismData& org)
{
    j = nlohmann::json{ { "organism_id", org.organism_id }, { "cell_indices", org.cell_indices } };
}

inline void from_json(const nlohmann::json& j, OrganismData& org)
{
    org.organism_id = j.at("organism_id").get<uint8_t>();
    org.cell_indices = j.at("cell_indices").get<std::vector<uint16_t>>();
}

// Note: RenderMessage is only serialized via zpp_bits (binary), not JSON.
// JSON serialization is not needed since it's sent as binary frames.

} // namespace DirtSim
