#include "MaterialType.h"

#include <array>
#include <cassert>
#include <cstddef>
#include <stdexcept>
#include <string>

// Material property database.
// Each material is defined using designated initializers for easy editing and understanding.
static std::array<MaterialProperties, 8> MATERIAL_PROPERTIES = { {
    // ========== AIR ==========
    // Nearly massless, high elasticity, no cohesion/adhesion, very high pressure diffusion.
    {
        .density                      = 0.001,
        .elasticity                   = 1.0,
        .cohesion                     = 0.0,
        .adhesion                     = 0.0,
        .com_mass_constant            = 0.0,
        .pressure_diffusion           = 1.0,
        .viscosity                    = 0.001,
        .motion_sensitivity           = 0.0,
        .static_friction_coefficient  = 1.0,
        .kinetic_friction_coefficient = 1.0,
        .stick_velocity               = 0.0,
        .friction_transition_width    = 0.01,
        .is_fluid                     = true,
        .is_rigid                     = false
    },

    // ========== DIRT ==========
    // Medium density granular material, forms clumps and stable slopes.
    // Resists flow until disturbed (avalanche behavior).
    {
        .density                      = 1.5,  // Medium weight granular material.
        .elasticity                   = 0.2,  // Low bounce, energy absorbed.
        .cohesion                     = 0.3,  // Forms clumps and stable slopes.
        .adhesion                     = 0.2,  // Moderate sticking to other materials.
        .com_mass_constant            = 5.0,  // Medium COM cohesion strength.
        .pressure_diffusion           = 0.3,  // Medium pressure propagation.
        .viscosity                    = 0.5,  // Resists flow moderately.
        .motion_sensitivity           = 0.0,  // Viscosity not affected by motion state.
        .static_friction_coefficient  = 1.0,  // Full resistance when at rest.
        .kinetic_friction_coefficient = 0.5,  // Half resistance when moving (avalanche).
        .stick_velocity               = 0.05, // Stays put until velocity exceeds 0.05.
        .friction_transition_width    = 0.10, // Gradual transition to flowing state.
        .is_fluid                     = false,
        .is_rigid                     = false
    },

    // ========== WATER ==========
    // Fluid with medium density, moderate cohesion for droplet formation.
    // High pressure diffusion, no static friction.
    {
        .density                      = 1.0,  // Standard fluid density.
        .elasticity                   = 0.1,  // Low bounce (inelastic).
        .cohesion                     = 0.25, // Moderate - forms droplets but flows freely.
        .adhesion                     = 0.5,  // Sticks to surfaces (wetting).
        .com_mass_constant            = 8.0,  // High COM cohesion strength.
        .pressure_diffusion           = 0.9,  // Very fast pressure equilibration.
        .viscosity                    = 0.01, // Flows very easily.
        .motion_sensitivity           = 1.0,  // Fully affected by motion state.
        .static_friction_coefficient  = 1.0,  // No static friction effect.
        .kinetic_friction_coefficient = 1.0,  // No kinetic friction effect.
        .stick_velocity               = 0.0,  // No stick velocity.
        .friction_transition_width    = 0.01, // Minimal transition.
        .is_fluid                     = true,
        .is_rigid                     = false
    },

    // ========== WOOD ==========
    // Light rigid material with moderate elasticity, low directional diffusion.
    // Sticky surface, maintains structure.
    {
        .density                      = 0.8,  // Light rigid material.
        .elasticity                   = 0.6,  // Moderate bounce.
        .cohesion                     = 0.7,  // Strong internal binding.
        .adhesion                     = 0.3,  // Moderate surface stickiness.
        .com_mass_constant            = 3.0,  // Low-medium COM cohesion.
        .pressure_diffusion           = 0.15, // Very slow (directional in real wood).
        .viscosity                    = 0.9,  // Essentially solid.
        .motion_sensitivity           = 0.2,  // Slightly affected by motion.
        .static_friction_coefficient  = 1.3,  // Sticky when at rest.
        .kinetic_friction_coefficient = 0.9,  // Moderate friction when moving.
        .stick_velocity               = 0.02, // Sharp breakaway.
        .friction_transition_width    = 0.03, // Moderate transition.
        .is_fluid                     = false,
        .is_rigid                     = true
    },

    // ========== SAND ==========
    // Dense granular material, settles faster than dirt.
    // Light resistance, flows when disturbed.
    {
        .density                      = 1.8,  // Dense granular material.
        .elasticity                   = 0.2,  // Low bounce.
        .cohesion                     = 0.2,  // Weak binding (granular).
        .adhesion                     = 0.1,  // Doesn't stick well.
        .com_mass_constant            = 4.0,  // Medium COM cohesion.
        .pressure_diffusion           = 0.3,  // Slow pressure propagation.
        .viscosity                    = 0.3,  // Granular flow resistance.
        .motion_sensitivity           = 0.5,  // Moderately affected by motion.
        .static_friction_coefficient  = 0.6,  // Light resistance at rest.
        .kinetic_friction_coefficient = 0.4,  // Light resistance when moving.
        .stick_velocity               = 0.04, // Flows when disturbed.
        .friction_transition_width    = 0.08, // Gradual transition.
        .is_fluid                     = false,
        .is_rigid                     = false
    },

    // ========== METAL ==========
    // Very dense rigid material with high elasticity and maximum cohesion.
    // Low diffusion, very sticky, essentially rigid body.
    {
        .density                      = 7.8,  // Very dense.
        .elasticity                   = 0.8,  // High bounce (elastic collisions).
        .cohesion                     = 1.0,  // Maximum binding strength.
        .adhesion                     = 0.1,  // Low surface adhesion.
        .com_mass_constant            = 2.0,  // Low COM cohesion (rigid body).
        .pressure_diffusion           = 0.1,  // Minimal pressure propagation.
        .viscosity                    = 0.95, // Essentially rigid.
        .motion_sensitivity           = 0.1,  // Barely affected by motion.
        .static_friction_coefficient  = 1.5,  // Very sticky when at rest.
        .kinetic_friction_coefficient = 1.0,  // Full friction when moving.
        .stick_velocity               = 0.01, // Very sharp breakaway.
        .friction_transition_width    = 0.02, // Sharp transition.
        .is_fluid                     = false,
        .is_rigid                     = true
    },

    // ========== LEAF ==========
    // Very light organic matter, medium-high diffusion due to porous structure.
    // Light material, easily affected by motion.
    {
        .density                      = 0.3,  // Very light.
        .elasticity                   = 0.4,  // Moderate bounce.
        .cohesion                     = 0.3,  // Light binding.
        .adhesion                     = 0.2,  // Moderate sticking.
        .com_mass_constant            = 10.0, // High COM cohesion (clusters).
        .pressure_diffusion           = 0.6,  // Moderate-high (porous structure).
        .viscosity                    = 0.2,  // Light resistance.
        .motion_sensitivity           = 0.8,  // Highly affected by motion.
        .static_friction_coefficient  = 0.5,  // Light materials.
        .kinetic_friction_coefficient = 0.3,  // Easy to move.
        .stick_velocity               = 0.03, // Moderate breakaway.
        .friction_transition_width    = 0.06, // Moderate transition.
        .is_fluid                     = false,
        .is_rigid                     = false
    },

    // ========== WALL ==========
    // Immobile boundary material (infinite effective density).
    // Zero pressure diffusion, acts as barrier and reflector.
    {
        .density                      = 1000.0, // Effectively infinite.
        .elasticity                   = 0.9,
        .cohesion                     = 1.0,
        .adhesion                     = 0.5,
        .com_mass_constant            = 0.0,    // N/A (immobile).
        .pressure_diffusion           = 0.0,    // Complete barrier.
        .viscosity                    = 1.0,
        .motion_sensitivity           = 0.0,
        .static_friction_coefficient  = 1.0,
        .kinetic_friction_coefficient = 1.0,
        .stick_velocity               = 0.0,
        .friction_transition_width    = 0.01,
        .is_fluid                     = false,
        .is_rigid                     = true
    }
} };

// Material name lookup table.
static const std::array<const char*, 8> MATERIAL_NAMES = {
    { "AIR", "DIRT", "WATER", "WOOD", "SAND", "METAL", "LEAF", "WALL" }
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

rapidjson::Value materialTypeToJson(
    MaterialType type, rapidjson::Document::AllocatorType& allocator)
{
    const char* name = getMaterialName(type);
    rapidjson::Value json(name, allocator);
    return json;
}

MaterialType materialTypeFromJson(const rapidjson::Value& json)
{
    if (!json.IsString()) {
        throw std::runtime_error("MaterialType::fromJson: JSON value must be a string");
    }

    std::string name = json.GetString();

    // Linear search through material names.
    for (size_t i = 0; i < MATERIAL_NAMES.size(); ++i) {
        if (name == MATERIAL_NAMES[i]) {
            return static_cast<MaterialType>(i);
        }
    }

    throw std::runtime_error("MaterialType::fromJson: Unknown material type '" + name + "'");
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
