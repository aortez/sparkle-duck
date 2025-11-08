#pragma once

#include "Cell.h"
#include "ReflectSerializer.h"
#include "ScenarioConfig.h"
#include <cstdint>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace DirtSim {

/**
 * @brief World state data - public source of truth.
 *
 * Simple aggregate struct - automatically serializable via ReflectSerializer.
 * All world state that needs to be saved/transmitted lives here.
 */
struct WorldData {
    // Grid dimensions and cells (1D storage for performance).
    uint32_t width = 0;
    uint32_t height = 0;
    std::vector<Cell> cells;  // Flat array: cells[y * width + x]

    // Simulation state.
    uint32_t timestep = 0;
    double timescale = 1.0;
    double removed_mass = 0.0;

    // Physics parameters.
    double gravity = 0.5;
    double elasticity_factor = 0.8;
    double pressure_scale = 1.0;

    // Feature flags.
    bool add_particles_enabled = true;

    // Scenario metadata and configuration.
    std::string scenario_id = "empty";
    ScenarioConfig scenario_config = EmptyConfig{};
};

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
