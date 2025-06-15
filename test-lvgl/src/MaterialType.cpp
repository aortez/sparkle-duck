#include "MaterialType.h"

#include <array>
#include <cassert>
#include <cstddef>

// Material property database
// Format: {density, elasticity, cohesion, adhesion, is_fluid, is_rigid}
static const std::array<MaterialProperties, 8> MATERIAL_PROPERTIES = {{
    // AIR: Nearly massless, high elasticity, no cohesion/adhesion
    {0.001, 1.0, 0.0, 0.0, true, false},
    
    // DIRT: Medium density granular material
    {1.5, 0.3, 0.4, 0.2, false, false},
    
    // WATER: Fluid with medium density, no elasticity when flowing
    {1.0, 0.1, 0.1, 0.1, true, false},
    
    // WOOD: Light rigid material with moderate elasticity
    {0.8, 0.6, 0.7, 0.3, false, true},
    
    // SAND: Dense granular material, settles faster than dirt
    {1.8, 0.2, 0.2, 0.1, false, false},
    
    // METAL: Very dense rigid material with high elasticity
    {7.8, 0.8, 0.9, 0.4, false, true},
    
    // LEAF: Very light organic matter
    {0.3, 0.4, 0.3, 0.2, false, false},
    
    // WALL: Immobile boundary material (infinite effective density)
    {1000.0, 0.9, 1.0, 1.0, false, true}
}};

// Material name lookup table
static const std::array<const char*, 8> MATERIAL_NAMES = {{
    "AIR",
    "DIRT", 
    "WATER",
    "WOOD",
    "SAND",
    "METAL",
    "LEAF",
    "WALL"
}};

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