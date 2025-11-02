#pragma once

#include <nlohmann/json.hpp>
#include <cstdint>

/**
 * \file
 * Material type definitions for the pure-material World physics system.
 * Each cell contains one material type with a fill ratio [0,1].
 */

enum class MaterialType : uint8_t {
    AIR = 0, // Empty space (default).
    DIRT,    // Granular solid material.
    WATER,   // Fluid material.
    WOOD,    // Rigid solid (light).
    SAND,    // Granular solid (faster settling than dirt).
    METAL,   // Dense rigid solid.
    LEAF,    // Light organic matter.
    WALL     // Immobile boundary material.
};

/**
 * Material properties that define physical behavior.
 */
struct MaterialProperties {
    double density;                      // Mass per unit volume (affects gravity response).
    double elasticity;                   // Bounce factor for collisions [0.0-1.0].
    double cohesion;                     // Internal binding strength (affects flow).
    double adhesion;                     // Binding strength to other materials.
    double com_mass_constant;            // Material-specific constant for mass-based COM cohesion.
    double pressure_diffusion;           // Pressure propagation rate [0.0-1.0].
    double viscosity;                    // Flow resistance [0.0-1.0].
    double motion_sensitivity;           // How much motion state affects viscosity [0.0-1.0].
    double static_friction_coefficient;  // Resistance multiplier when at rest (typically 1.0-1.5).
    double kinetic_friction_coefficient; // Resistance multiplier when moving (typically 0.4-1.0).
    double stick_velocity; // Velocity below which full static friction applies (0.0-0.05).
    double friction_transition_width; // How quickly friction transitions from static to kinetic
                                      // (0.02-0.1).
    bool is_fluid;                    // True for materials that flow freely.
    bool is_rigid;                    // True for materials that only compress, don't flow.
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
 * Set the cohesion value for a specific material type.
 * This allows dynamic modification of material properties.
 */
void setMaterialCohesion(MaterialType type, double cohesion);

/**
 * Calculate velocity-dependent friction coefficient with smooth transition.
 * Returns a value between kinetic and static friction coefficients based on velocity.
 */
double getFrictionCoefficient(double velocity_magnitude, const MaterialProperties& props);

/**
 * JSON serialization support for MaterialType (ADL convention for nlohmann::json).
 */
void to_json(nlohmann::json& j, MaterialType type);
void from_json(const nlohmann::json& j, MaterialType& type);