#include "MaterialType.h"

#include <array>
#include <cassert>
#include <cstddef>
#include <stdexcept>
#include <string>

// Material property database.
// Format: {density, elasticity, cohesion, adhesion, com_mass_constant, is_fluid, is_rigid}
static std::array<MaterialProperties, 8> MATERIAL_PROPERTIES = {
    { // AIR: Nearly massless, high elasticity, no cohesion/adhesion.
      { 0.001, 1.0, 0.0, 0.0, 0.0, true, false },

      // DIRT: Medium density granular material.
      { 1.5, 0.3, 0.3, 0.2, 5.0, false, false },

      // WATER: Fluid with medium density, strong cohesion for droplet formation.
      { 1.0, 0.1, 0.6, 0.5, 8.0, true, false },

      // WOOD: Light rigid material with moderate elasticity.
      { 0.8, 0.6, 0.7, 0.3, 3.0, false, true },

      // SAND: Dense granular material, settles faster than dirt.
      { 1.8, 0.2, 0.2, 0.1, 4.0, false, false },

      // METAL: Very dense rigid material with high elasticity and maximum cohesion.
      { 7.8, 0.8, 1.0, 0.1, 2.0, false, true },

      // LEAF: Very light organic matter.
      { 0.3, 0.4, 0.3, 0.2, 10.0, false, false },

      // WALL: Immobile boundary material (infinite effective density)
      { 1000.0, 0.9, 1.0, 1.0, 0.0, false, true } }
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
