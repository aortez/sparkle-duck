#include "PhysicsSettings.h"

namespace DirtSim {

/**
 * @brief Get default physics settings.
 *
 * This function is in a .cpp file to avoid recompiling many files when
 * tweaking default values during experimentation.
 */
PhysicsSettings getDefaultPhysicsSettings()
{
    return PhysicsSettings{ .adhesion_strength = 2.0,
                            .adhesion_enabled = true,
                            .air_resistance = 0.1,
                            .buoyancy_energy_scale = 1.0,
                            .cohesion_resistance_factor = 25.0,
                            .cohesion_strength = 10.0,
                            .cohesion_enabled = true,
                            .elasticity = 0.8,
                            .fluid_lubrication_factor = 0.5,
                            .fragmentation_enabled = true,
                            .fragmentation_threshold = 5.0,
                            .fragmentation_full_threshold = 10.0,
                            .fragmentation_spray_fraction = 0.4,
                            .friction_strength = 1.0,
                            .friction_enabled = true,
                            .gravity = 9.81,
                            .horizontal_flow_resistance_factor = 1.0,
                            .horizontal_non_fluid_penalty = 0.05,
                            .horizontal_non_fluid_target_resistance = 10.0,
                            .horizontal_non_fluid_energy_multiplier = 10.0,
                            .pressure_dynamic_strength = 0.3,
                            .pressure_dynamic_enabled = true,
                            .pressure_hydrostatic_strength = 1.0,
                            .pressure_hydrostatic_enabled = true,
                            .pressure_scale = 1.0,
                            .pressure_diffusion_strength = 10.0,
                            .pressure_diffusion_iterations = 2,
                            .pressure_decay_rate = 0.20,
                            .swap_enabled = true,
                            .timescale = 1.0,
                            .viscosity_strength = 1.0,
                            .viscosity_enabled = true };
}

} // namespace DirtSim
