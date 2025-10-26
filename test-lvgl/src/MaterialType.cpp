#include "MaterialType.h"

#include <array>
#include <cassert>
#include <cstddef>
#include <stdexcept>
#include <string>

// Material property database.
// Format: {density, elasticity, cohesion, adhesion, com_mass_constant, pressure_diffusion,
// viscosity, motion_sensitivity, static_friction, kinetic_friction, stick_velocity,
// friction_transition_width, is_fluid, is_rigid}
static std::array<MaterialProperties, 8> MATERIAL_PROPERTIES = {
    { // AIR: Nearly massless, high elasticity, no cohesion/adhesion, very high pressure diffusion,
      // no friction.
      { 0.001, 1.0, 0.0, 0.0, 0.0, 1.0, 0.001, 0.0, 1.0, 1.0, 0.0, 0.01, true, false },

      // DIRT: Medium density granular material, medium pressure diffusion, resists flow until
      // disturbed.
      {
          1.5,   // density - medium weight granular material
          0.3,   // elasticity - low bounce, energy absorbed
          0.3,   // cohesion - forms clumps and stable slopes
          0.2,   // adhesion - moderate sticking to other materials
          5.0,   // com_mass_constant - medium COM cohesion strength
          0.4,   // pressure_diffusion - medium pressure propagation
          0.5,   // viscosity - resists flow moderately
          0.0,   // motion_sensitivity - viscosity heavily affected by motion state
          1.0,   // static_friction_coefficient - full resistance when at rest
          0.5,   // kinetic_friction_coefficient - half resistance when moving (avalanche behavior)
          0.05,     // stick_velocity - stays put until velocity exceeds 0.05
          0.10,  // friction_transition_width - gradual transition to flowing state
          false, // is_fluid - not a fluid
          false  // is_rigid - deformable granular material
      },
      //      {
      //          1.5,    // density - medium weight granular material
      //          0.3,    // elasticity - low bounce, energy absorbed
      //          0.3,    // cohesion - forms clumps and stable slopes
      //          0.2,    // adhesion - moderate sticking to other materials
      //          5.0,    // com_mass_constant - medium COM cohesion strength
      //          0.4,    // pressure_diffusion - medium pressure propagation
      //          0.5,    // viscosity - resists flow moderately
      //          0.99,   // motion_sensitivity - viscosity heavily affected by motion state
      //          1.0,    // static_friction_coefficient - full resistance when at rest
      //          0.5,    // kinetic_friction_coefficient - half resistance when moving (avalanche
      //          behavior)
      //          0.05,   // stick_velocity - stays put until velocity exceeds 0.05
      //          0.10,   // friction_transition_width - gradual transition to flowing state
      //          false,  // is_fluid - not a fluid
      //          false   // is_rigid - deformable granular material
      //      },

      // WATER: Fluid with medium density, strong cohesion for droplet formation, high pressure
      // diffusion, no static friction.
      { 1.0, 0.1, 0.6, 0.5, 8.0, 0.9, 0.01, 1.0, 1.0, 1.0, 0.0, 0.01, true, false },

      // WOOD: Light rigid material with moderate elasticity, low directional diffusion, sticky
      // surface.
      { 0.8, 0.6, 0.7, 0.3, 3.0, 0.15, 0.9, 0.2, 1.3, 0.9, 0.02, 0.03, false, true },

      // SAND: Dense granular material, settles faster than dirt, medium pressure diffusion, light
      // resistance.
      { 1.8, 0.2, 0.2, 0.1, 4.0, 0.3, 0.3, 0.5, 0.6, 0.4, 0.04, 0.08, false, false },

      // METAL: Very dense rigid material with high elasticity and maximum cohesion, low diffusion,
      // very sticky.
      { 7.8, 0.8, 1.0, 0.1, 2.0, 0.1, 0.95, 0.1, 1.5, 1.0, 0.01, 0.02, false, true },

      // LEAF: Very light organic matter, medium-high diffusion due to porous structure, light
      // material.
      { 0.3, 0.4, 0.3, 0.2, 10.0, 0.6, 0.2, 0.8, 0.5, 0.3, 0.03, 0.06, false, false },

      // WALL: Immobile boundary material (infinite effective density), zero pressure diffusion, N/A
      // friction.
      { 1000.0, 0.9, 1.0, 1.0, 0.0, 0.0, 1.0, 0.0, 1.0, 1.0, 0.0, 0.01, false, true } }
};

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
