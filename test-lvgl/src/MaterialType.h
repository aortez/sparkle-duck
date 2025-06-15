#pragma once

#include <cstdint>
#include "lvgl/src/libs/thorvg/rapidjson/document.h"

/**
 * \file
 * Material type definitions for the pure-material WorldB physics system.
 * Each cell contains one material type with a fill ratio [0,1].
 */

enum class MaterialType : uint8_t {
    AIR = 0,    // Empty space (default)
    DIRT,       // Granular solid material
    WATER,      // Fluid material
    WOOD,       // Rigid solid (light)
    SAND,       // Granular solid (faster settling than dirt)
    METAL,      // Dense rigid solid
    LEAF,       // Light organic matter
    WALL        // Immobile boundary material
};

/**
 * Material properties that define physical behavior.
 */
struct MaterialProperties {
    double density;      // Mass per unit volume (affects gravity response)
    double elasticity;   // Bounce factor for collisions [0.0-1.0]
    double cohesion;     // Internal binding strength (affects flow)
    double adhesion;     // Binding strength to other materials
    bool is_fluid;       // True for materials that flow freely
    bool is_rigid;       // True for materials that only compress, don't flow
    
    MaterialProperties(double d, double e, double c, double a, bool fluid, bool rigid)
        : density(d), elasticity(e), cohesion(c), adhesion(a), is_fluid(fluid), is_rigid(rigid) {}
};

/**
 * Get material properties for a given material type.
 */
const MaterialProperties& getMaterialProperties(MaterialType type);

/**
 * Get the density of a material type.
 */
double getMaterialDensity(MaterialType type);

/**
 * Check if a material is a fluid.
 */
bool isMaterialFluid(MaterialType type);

/**
 * Check if a material is rigid (compression-only).
 */
bool isMaterialRigid(MaterialType type);

/**
 * Get a human-readable name for a material type.
 */
const char* getMaterialName(MaterialType type);

/**
 * JSON serialization support for MaterialType.
 */
rapidjson::Value materialTypeToJson(MaterialType type, rapidjson::Document::AllocatorType& allocator);
MaterialType materialTypeFromJson(const rapidjson::Value& json);