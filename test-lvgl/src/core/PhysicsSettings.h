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
    // General physics.
    double timescale = 1.0;
    double gravity = 0.5;
    double elasticity = 0.8;
    double air_resistance = 0.1;

    // Pressure systems.
    bool hydrostatic_pressure_enabled = true;
    double hydrostatic_pressure_strength = 1.0;
    bool dynamic_pressure_enabled = true;
    double dynamic_pressure_strength = 1.0;
    bool pressure_diffusion_enabled = true;

    // Force parameters.
    bool cohesion_force_enabled = true;
    double cohesion_force_strength = 150.0;
    bool adhesion_enabled = true;
    double adhesion_strength = 5.0;
    bool viscosity_enabled = true;
    double viscosity_strength = 1.0;
    bool friction_enabled = true;
    double friction_strength = 1.0;
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
