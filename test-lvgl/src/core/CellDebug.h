#pragma once

#include "ReflectSerializer.h"
#include "Vector2d.h"
#include <nlohmann/json.hpp>

namespace DirtSim {

/**
 * CellDebug: Debug information for visualization/analysis.
 *
 * Stored separately from Cell to keep core physics data compact.
 * Owned by WorldData, referenced by GridOfCells for fast access.
 */
struct CellDebug {
    // Force accumulation for visualization.
    Vector2d accumulated_viscous_force = {};
    Vector2d accumulated_adhesion_force = {};
    Vector2d accumulated_com_cohesion_force = {};
    Vector2d accumulated_friction_force = {};

    // Physics debug values.
    double damping_factor = 1.0;              // Effective damping applied to velocity.
    double cohesion_resistance = 0.0;         // Cohesion resistance threshold.
    double cached_friction_coefficient = 1.0; // Friction coefficient used this frame.
};

/**
 * ADL functions for automatic JSON serialization.
 */
inline void to_json(nlohmann::json& j, const CellDebug& debug)
{
    j = ReflectSerializer::to_json(debug);
}

inline void from_json(const nlohmann::json& j, CellDebug& debug)
{
    debug = ReflectSerializer::from_json<CellDebug>(j);
}

} // namespace DirtSim
