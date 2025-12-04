#pragma once

#include "ReflectSerializer.h"
#include <nlohmann/json.hpp>

namespace DirtSim {

/**
 * @brief Physics simulation parameters.
 *
 * Centralized settings for all physics simulation parameters.
 * Automatically serializable via ReflectSerializer.
 */
struct PhysicsSettings {
    double adhesion_strength = 2.0;
    bool adhesion_enabled = true;
    double air_resistance = 0.1;
    double buoyancy_energy_scale = 1.0;
    double cohesion_resistance_factor = 25.0;
    double cohesion_strength = 10.0;
    bool cohesion_enabled = true;
    double elasticity = 0.8;
    double fluid_lubrication_factor = 0.5;
    bool fragmentation_enabled = true;
    double fragmentation_threshold = 5.0;       // Minimum energy for fragmentation chance.
    double fragmentation_full_threshold = 10.0; // Energy for 100% fragmentation.
    double fragmentation_spray_fraction = 0.4;  // Fraction of fill_ratio that sprays out.
    double friction_strength = 1.0;
    bool friction_enabled = true;
    double gravity = 9.81;
    double horizontal_flow_resistance_factor = 1;
    double horizontal_non_fluid_penalty = 0.05;
    double horizontal_non_fluid_target_resistance = 10.0;
    double non_fluid_energy_multiplier = 10.0;
    double pressure_dynamic_strength = 0.3;
    bool pressure_dynamic_enabled = true;
    double pressure_hydrostatic_strength = 1.0;
    bool pressure_hydrostatic_enabled = true;
    double pressure_scale = 1.0;
    double pressure_diffusion_strength = 5.0;
    int pressure_diffusion_iterations = 2;
    bool swap_enabled = true;
    double timescale = 1.0;
    double viscosity_strength = 1.0;
    bool viscosity_enabled = true;
};

/**
 * ADL functions for automatic JSON conversion.
 */
inline void to_json(nlohmann::json& j, const PhysicsSettings& settings)
{
    j = ReflectSerializer::to_json(settings);
}

inline void from_json(const nlohmann::json& j, PhysicsSettings& settings)
{
    settings = ReflectSerializer::from_json<PhysicsSettings>(j);
}

} // namespace DirtSim
