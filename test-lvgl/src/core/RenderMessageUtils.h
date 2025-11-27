#pragma once

#include "Cell.h"
#include "RenderMessage.h"
#include "WorldData.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <vector>

namespace DirtSim {

/**
 * @brief Utility functions for packing and unpacking RenderMessage data.
 *
 * Provides conversions between full Cell data and optimized BasicCell/DebugCell formats.
 */
namespace RenderMessageUtils {

// =============================================================================
// PACKING FUNCTIONS (Cell → BasicCell/DebugCell)
// =============================================================================

/**
 * @brief Pack a Cell into BasicCell format (2 bytes).
 *
 * Quantizes fill_ratio to 8-bit precision.
 */
inline BasicCell packBasicCell(const Cell& cell)
{
    BasicCell result;
    result.material_type = static_cast<uint8_t>(cell.material_type);
    result.fill_ratio = static_cast<uint8_t>(std::clamp(cell.fill_ratio * 255.0, 0.0, 255.0));
    return result;
}

/**
 * @brief Pack a Cell into DebugCell format (16 bytes).
 *
 * Quantizes all floating-point values to fixed-point integers.
 * - COM: [-1.0, 1.0] → int16_t [-32767, 32767]
 * - Velocity: [-10.0, 10.0] → int16_t [-32767, 32767]
 * - Pressure: [0, 1000] → uint16_t [0, 65535]
 */
inline DebugCell packDebugCell(const Cell& cell)
{
    DebugCell result;
    result.material_type = static_cast<uint8_t>(cell.material_type);
    result.fill_ratio = static_cast<uint8_t>(std::clamp(cell.fill_ratio * 255.0, 0.0, 255.0));
    result._padding[0] = 0;
    result._padding[1] = 0;

    // COM: [-1.0, 1.0] → [-32767, 32767].
    result.com_x = static_cast<int16_t>(std::clamp(cell.com.x * 32767.0, -32767.0, 32767.0));
    result.com_y = static_cast<int16_t>(std::clamp(cell.com.y * 32767.0, -32767.0, 32767.0));

    // Velocity: assume max ±10.0 units/sec → [-32767, 32767].
    constexpr double velocity_scale = 32767.0 / 10.0;
    result.velocity_x =
        static_cast<int16_t>(std::clamp(cell.velocity.x * velocity_scale, -32767.0, 32767.0));
    result.velocity_y =
        static_cast<int16_t>(std::clamp(cell.velocity.y * velocity_scale, -32767.0, 32767.0));

    // Pressure: [0, 1000] → [0, 65535].
    constexpr double pressure_scale = 65535.0 / 1000.0;
    result.pressure_hydro = static_cast<uint16_t>(
        std::clamp(cell.hydrostatic_component * pressure_scale, 0.0, 65535.0));
    result.pressure_dynamic =
        static_cast<uint16_t>(std::clamp(cell.dynamic_component * pressure_scale, 0.0, 65535.0));

    // Pressure gradient: stored as float directly.
    result.pressure_gradient.x = static_cast<float>(cell.pressure_gradient.x);
    result.pressure_gradient.y = static_cast<float>(cell.pressure_gradient.y);

    return result;
}

/**
 * @brief Pack all cells from WorldData into BasicCell format.
 *
 * Returns a byte vector suitable for RenderMessage::payload.
 */
inline std::vector<std::byte> packBasicCells(const WorldData& data)
{
    std::vector<BasicCell> cells;
    cells.reserve(data.cells.size());

    for (const auto& cell : data.cells) {
        cells.push_back(packBasicCell(cell));
    }

    // Convert to byte vector.
    std::vector<std::byte> payload(cells.size() * sizeof(BasicCell));
    std::memcpy(payload.data(), cells.data(), payload.size());
    return payload;
}

/**
 * @brief Pack all cells from WorldData into DebugCell format.
 *
 * Returns a byte vector suitable for RenderMessage::payload.
 */
inline std::vector<std::byte> packDebugCells(const WorldData& data)
{
    std::vector<DebugCell> cells;
    cells.reserve(data.cells.size());

    for (const auto& cell : data.cells) {
        cells.push_back(packDebugCell(cell));
    }

    // Convert to byte vector.
    std::vector<std::byte> payload(cells.size() * sizeof(DebugCell));
    std::memcpy(payload.data(), cells.data(), payload.size());
    return payload;
}

/**
 * @brief Extract sparse organism data from WorldData.
 *
 * Groups cells by organism_id and returns sparse representation.
 */
inline std::vector<OrganismData> extractOrganisms(const WorldData& data)
{
    std::map<uint32_t, std::vector<uint16_t>> organism_map;

    // Group cells by organism ID.
    for (size_t i = 0; i < data.cells.size(); ++i) {
        uint32_t org_id = data.cells[i].organism_id;
        if (org_id != 0) {
            organism_map[org_id].push_back(static_cast<uint16_t>(i));
        }
    }

    // Convert to OrganismData vector.
    std::vector<OrganismData> result;
    result.reserve(organism_map.size());

    for (const auto& [id, indices] : organism_map) {
        OrganismData org;
        org.organism_id = static_cast<uint8_t>(id);
        org.cell_indices = indices;
        result.push_back(std::move(org));
    }

    return result;
}

/**
 * @brief Pack WorldData into RenderMessage with specified format.
 */
inline RenderMessage packRenderMessage(const WorldData& data, RenderFormat format)
{
    RenderMessage msg;
    msg.format = format;
    msg.width = data.width;
    msg.height = data.height;
    msg.timestep = data.timestep;
    msg.fps_server = data.fps_server;
    msg.scenario_id = data.scenario_id;
    msg.scenario_config = data.scenario_config;
    msg.tree_vision = data.tree_vision;

    // Pack cells based on format.
    if (format == RenderFormat::BASIC) {
        msg.payload = packBasicCells(data);
    }
    else if (format == RenderFormat::DEBUG) {
        msg.payload = packDebugCells(data);
    }

    // Extract sparse organism data.
    msg.organisms = extractOrganisms(data);

    return msg;
}

// =============================================================================
// UNPACKING FUNCTIONS (BasicCell/DebugCell → rendering data)
// =============================================================================

/**
 * @brief Unpack BasicCell to get material type and fill ratio.
 */
inline void unpackBasicCell(const BasicCell& src, MaterialType& material, double& fill_ratio)
{
    material = static_cast<MaterialType>(src.material_type);
    fill_ratio = src.fill_ratio / 255.0;
}

/**
 * @brief Unpack DebugCell to get all rendering data.
 */
struct UnpackedDebugCell {
    MaterialType material_type;
    double fill_ratio;
    Vector2d com;
    Vector2d velocity;
    double pressure_hydro;
    double pressure_dynamic;
    Vector2d pressure_gradient;
};

inline UnpackedDebugCell unpackDebugCell(const DebugCell& src)
{
    UnpackedDebugCell result;

    result.material_type = static_cast<MaterialType>(src.material_type);
    result.fill_ratio = src.fill_ratio / 255.0;

    // COM: [-32767, 32767] → [-1.0, 1.0].
    result.com.x = src.com_x / 32767.0;
    result.com.y = src.com_y / 32767.0;

    // Velocity: [-32767, 32767] → [-10.0, 10.0].
    constexpr double velocity_scale = 10.0 / 32767.0;
    result.velocity.x = src.velocity_x * velocity_scale;
    result.velocity.y = src.velocity_y * velocity_scale;

    // Pressure: [0, 65535] → [0, 1000].
    constexpr double pressure_scale = 1000.0 / 65535.0;
    result.pressure_hydro = src.pressure_hydro * pressure_scale;
    result.pressure_dynamic = src.pressure_dynamic * pressure_scale;

    // Pressure gradient: stored as float, convert to double.
    result.pressure_gradient.x = static_cast<double>(src.pressure_gradient.x);
    result.pressure_gradient.y = static_cast<double>(src.pressure_gradient.y);

    return result;
}

/**
 * @brief Apply sparse organism data to a cell array.
 *
 * Fills organism_ids array (same size as cells) based on sparse OrganismData.
 */
inline std::vector<uint8_t> applyOrganismData(
    const std::vector<OrganismData>& organisms, size_t num_cells)
{
    std::vector<uint8_t> organism_ids(num_cells, 0);

    for (const auto& org : organisms) {
        for (uint16_t idx : org.cell_indices) {
            if (idx < num_cells) {
                organism_ids[idx] = org.organism_id;
            }
        }
    }

    return organism_ids;
}

} // namespace RenderMessageUtils
} // namespace DirtSim
