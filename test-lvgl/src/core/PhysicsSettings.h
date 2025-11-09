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
    double adhesion_strength = 5.0;
    bool adhesion_enabled = true;
    double air_resistance = 0.1;
    double cohesion_strength = 150.0;
    bool cohesion_enabled = true;
    double elasticity = 0.8;
    double friction_strength = 1.0;
    bool friction_enabled = true;
    double gravity = 0.5;
    double pressure_dynamic_strength = 1.0;
    bool pressure_dynamic_enabled = true;
    double pressure_hydrostatic_strength = 1.0;
    bool pressure_hydrostatic_enabled = true;
    double pressure_scale = 1.0;
    bool pressure_diffusion_enabled = true;
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
