#include "MaterialType.h"

#include <array>
#include <cassert>
#include <cstddef>
#include <stdexcept>
#include <string>

namespace DirtSim {

// Material property database.
// Each material is defined using designated initializers for easy editing and understanding.
static std::array<MaterialProperties, 10> MATERIAL_PROPERTIES = {
    { // ========== AIR ==========
      // Nearly massless, high elasticity, no cohesion/adhesion, very high pressure diffusion.
      { .density = 0.001,
        .elasticity = 1.0,
        .cohesion = 0.0,
        .adhesion = 0.0,
        .air_resistance = 0.0,
        .hydrostatic_weight = 1.0,
        .dynamic_weight = 0.0,
        .pressure_diffusion = 1.0,
        .viscosity = 0.001,
        .motion_sensitivity = 0.0,
        .static_friction_coefficient = 1.0,
        .kinetic_friction_coefficient = 1.0,
        .stick_velocity = 0.0,
        .friction_transition_width = 0.01,
        .is_fluid = true,
        .is_rigid = false },

      // ========== DIRT ==========
      { .density = 1.5,
        .elasticity = 0.2,
        .cohesion = 0.5,
        .adhesion = 0.2,
        .air_resistance = 0.1,
        .hydrostatic_weight = 0.25,
        .dynamic_weight = 1.0,
        .pressure_diffusion = 0.3,
        .viscosity = 0.5,
        .motion_sensitivity = 0.0,
        .static_friction_coefficient = 1.0,
        .kinetic_friction_coefficient = 0.5,
        .stick_velocity = 0.5,
        .friction_transition_width = 0.10,
        .is_fluid = false,
        .is_rigid = false },

      // ========== LEAF ==========
      { .density = 0.3,
        .elasticity = 0.4,
        .cohesion = 0.7,
        .adhesion = 0.3,
        .air_resistance = 0.8,
        .hydrostatic_weight = 1.0,
        .dynamic_weight = 0.6,
        .pressure_diffusion = 0.6,
        .viscosity = 0.2,
        .motion_sensitivity = 0.8,
        .static_friction_coefficient = 0.5,
        .kinetic_friction_coefficient = 0.3,
        .stick_velocity = 0.03,
        .friction_transition_width = 0.06,
        .is_fluid = false,
        .is_rigid = false },

      // ========== METAL ==========
      { .density = 7.8,
        .elasticity = 0.8,
        .cohesion = 1.0,
        .adhesion = 0.1,
        .air_resistance = 0.1,
        .hydrostatic_weight = 0.0, // Rigid materials don't respond to pressure.
        .dynamic_weight = 0.5,
        .pressure_diffusion = 0.1,
        .viscosity = 1,
        .motion_sensitivity = 0.1,
        .static_friction_coefficient = 1.5,
        .kinetic_friction_coefficient = 1.0,
        .stick_velocity = 0.01,
        .friction_transition_width = 0.02,
        .is_fluid = false,
        .is_rigid = true },

      // ========== ROOT ==========
      // Underground tree tissue that grips soil and forms networks.
      { .density = 1.2,
        .elasticity = 0.3,
        .cohesion = 0.8,
        .adhesion = 0.6,
        .air_resistance = 0.3,
        .hydrostatic_weight = 1.0,
        .dynamic_weight = 0.7,
        .pressure_diffusion = 0.4,
        .viscosity = 0.7,
        .motion_sensitivity = 0.3,
        .static_friction_coefficient = 1.2,
        .kinetic_friction_coefficient = 0.8,
        .stick_velocity = 0.03,
        .friction_transition_width = 0.05,
        .is_fluid = false,
        .is_rigid = false },

      // ========== SAND ==========
      { .density = 1.8,
        .elasticity = 0.2,
        .cohesion = 0.2,
        .adhesion = 0.1,
        .air_resistance = 0.2,
        .hydrostatic_weight = 1.0,
        .dynamic_weight = 1.0,
        .pressure_diffusion = 0.3,
        .viscosity = 0.3,
        .motion_sensitivity = 0.5,
        .static_friction_coefficient = 0.6,
        .kinetic_friction_coefficient = 0.4,
        .stick_velocity = 0.04,
        .friction_transition_width = 0.08,
        .is_fluid = false,
        .is_rigid = false },

      // ========== SEED ==========
      { .density = 1.5,
        .elasticity = 0.2,
        .cohesion = 0.9,
        .adhesion = 0.3,
        .air_resistance = 0.2,
        .hydrostatic_weight = 0.0, // Rigid materials don't respond to pressure.
        .dynamic_weight = 0.5,
        .pressure_diffusion = 0.1,
        .viscosity = 0.8,
        .motion_sensitivity = 0.1,
        .static_friction_coefficient = 1.3,
        .kinetic_friction_coefficient = 0.9,
        .stick_velocity = 0.02,
        .friction_transition_width = 0.03,
        .is_fluid = false,
        .is_rigid = true },

      // ========== WALL ==========
      { .density = 1000.0,
        .elasticity = 0.9,
        .cohesion = 1.0,
        .adhesion = 0.5,
        .air_resistance = 0.0,
        .hydrostatic_weight = 0.0, // Rigid materials don't respond to pressure.
        .dynamic_weight = 0.0,
        .pressure_diffusion = 0.0,
        .viscosity = 1.0,
        .motion_sensitivity = 0.0,
        .static_friction_coefficient = 1.0,
        .kinetic_friction_coefficient = 1.0,
        .stick_velocity = 0.0,
        .friction_transition_width = 0.01,
        .is_fluid = false,
        .is_rigid = true },

      // ========== WATER ==========
      { .density = 1.0,
        .elasticity = 0.1,
        .cohesion = 0.1,
        .adhesion = 0.3,
        .air_resistance = 0.01,
        .hydrostatic_weight = 1.0,
        .dynamic_weight = 0.8,
        .pressure_diffusion = 0.9,
        .viscosity = 0.01,
        .motion_sensitivity = 1.0,
        .static_friction_coefficient = 1.0,
        .kinetic_friction_coefficient = 1.0,
        .stick_velocity = 0.0,
        .friction_transition_width = 0.01,
        .is_fluid = true,
        .is_rigid = false },

      // ========== WOOD ==========
      { .density = 0.3,
        .elasticity = 0.6,
        .cohesion = 0.7,
        .adhesion = 0.3,
        .air_resistance = 0.2,
        .hydrostatic_weight = 0.0,
        .dynamic_weight = 0.5,
        .pressure_diffusion = 0.15,
        .viscosity = 1,
        .motion_sensitivity = 0.2,
        .static_friction_coefficient = 1.3,
        .kinetic_friction_coefficient = 0.9,
        .stick_velocity = 0.02,
        .friction_transition_width = 0.03,
        .is_fluid = false,
        .is_rigid = true } }
};

// Material name lookup table.
static const std::array<const char*, 10> MATERIAL_NAMES = {
    { "AIR", "DIRT", "LEAF", "METAL", "ROOT", "SAND", "SEED", "WALL", "WATER", "WOOD" }
};

const MaterialProperties& getMaterialProperties(MaterialType type)
{
    const auto index = static_cast<size_t>(type);
    assert(index < MATERIAL_PROPERTIES.size());
    return MATERIAL_PROPERTIES[index];
}

double getMaterialDensity(MaterialType type)
{
    return getMaterialProperties(type).density;
}

bool isMaterialFluid(MaterialType type)
{
    return getMaterialProperties(type).is_fluid;
}

bool isMaterialRigid(MaterialType type)
{
    return getMaterialProperties(type).is_rigid;
}

const char* getMaterialName(MaterialType type)
{
    const auto index = static_cast<size_t>(type);
    assert(index < MATERIAL_NAMES.size());
    return MATERIAL_NAMES[index];
}

void to_json(nlohmann::json& j, MaterialType type)
{
    j = getMaterialName(type);
}

void from_json(const nlohmann::json& j, MaterialType& type)
{
    if (!j.is_string()) {
        throw std::runtime_error("MaterialType::from_json: JSON value must be a string");
    }

    std::string name = j.get<std::string>();

    // Linear search through material names.
    for (size_t i = 0; i < MATERIAL_NAMES.size(); ++i) {
        if (name == MATERIAL_NAMES[i]) {
            type = static_cast<MaterialType>(i);
            return;
        }
    }

    throw std::runtime_error("MaterialType::from_json: Unknown material type '" + name + "'");
}

void setMaterialCohesion(MaterialType type, double cohesion)
{
    const auto index = static_cast<size_t>(type);
    assert(index < MATERIAL_PROPERTIES.size());
    MATERIAL_PROPERTIES[index].cohesion = cohesion;
}

double getFrictionCoefficient(double velocity_magnitude, const MaterialProperties& props)
{
    // Below stick velocity, use full static friction.
    if (velocity_magnitude < props.stick_velocity) {
        return props.static_friction_coefficient;
    }

    // Calculate smooth transition parameter.
    double t = (velocity_magnitude - props.stick_velocity) / props.friction_transition_width;

    // Clamp t to [0, 1] range.
    if (t < 0.0) t = 0.0;
    if (t > 1.0) t = 1.0;

    // Smooth cubic interpolation (3t² - 2t³).
    double smooth_t = t * t * (3.0 - 2.0 * t);

    // Interpolate between static and kinetic friction.
    return props.static_friction_coefficient * (1.0 - smooth_t)
        + props.kinetic_friction_coefficient * smooth_t;
}
} // namespace DirtSim
