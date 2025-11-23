#pragma once

#include "Cell.h"
#include "ReflectSerializer.h"
#include "ScenarioConfig.h"
#include "organisms/TreeSensoryData.h"

#include <cstdint>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <vector>
#include <zpp_bits.h>

namespace DirtSim {

struct WorldData {
    // Grid dimensions and cells (1D storage for performance).
    uint32_t width = 0;
    uint32_t height = 0;
    std::vector<Cell> cells; // Flat array: cells[y * width + x]

    // Simulation state.
    uint32_t timestep = 0;
    double removed_mass = 0.0;
    double fps_server = 0.0;

    // Feature flags.
    bool add_particles_enabled = true;

    // Scenario metadata and configuration.
    std::string scenario_id = "empty";
    ScenarioConfig scenario_config = EmptyConfig{};

    // Tree organism data (optional - only present when showing a tree's vision).
    std::optional<TreeSensoryData> tree_vision;

    // Direct cell access methods (inline for performance).
    inline Cell& at(uint32_t x, uint32_t y)
    {
        assert(x < width && y < height);
        return cells[y * width + x];
    }

    inline const Cell& at(uint32_t x, uint32_t y) const
    {
        assert(x < width && y < height);
        return cells[y * width + x];
    }

    // Custom zpp_bits serialization (all 10 fields).
    using serialize = zpp::bits::members<10>;
};

/**
 * Optional serialization helpers for nlohmann::json.
 */
template <typename T>
void to_json(nlohmann::json& j, const std::optional<T>& opt)
{
    if (opt.has_value()) {
        j = opt.value();
    }
    else {
        j = nullptr;
    }
}

template <typename T>
void from_json(const nlohmann::json& j, std::optional<T>& opt)
{
    if (j.is_null()) {
        opt.reset();
    }
    else {
        opt = j.get<T>();
    }
}

/**
 * ADL (Argument-Dependent Lookup) functions for nlohmann::json automatic conversion.
 */
inline void to_json(nlohmann::json& j, const WorldData& data)
{
    j = ReflectSerializer::to_json(data);
}

inline void from_json(const nlohmann::json& j, WorldData& data)
{
    data = ReflectSerializer::from_json<WorldData>(j);
}

} // namespace DirtSim
