#pragma once

#include "MaterialType.h"
#include "Vector2d.h"

namespace DirtSim {

/**
 * @brief Types of collisions that can occur during material transfer
 *
 * This enum defines how materials interact when they collide during
 * movement in the World physics simulation.
 */
enum class CollisionType {
    TRANSFER_ONLY,       // Material moves between cells (default behavior)
    ELASTIC_REFLECTION,  // Bouncing with energy conservation
    INELASTIC_COLLISION, // Bouncing with energy loss
    FRAGMENTATION,       // Break apart into smaller pieces
    ABSORPTION           // One material absorbs the other
};

/**
 * @brief Represents a material transfer between cells with collision physics
 *
 * This struct encapsulates all data needed to perform a material transfer
 * including collision detection, energy calculations, and physics responses.
 * It supports both simple transfers and complex collision interactions.
 */
struct MaterialMove {
    // Basic transfer data (optimized layout for packing)
    double amount;            // Amount of material to transfer
    Vector2d momentum;        // Velocity/momentum of the moving material
    Vector2d boundary_normal; // Direction of boundary crossing for physics
    int fromX, fromY;         // Source cell coordinates
    int toX, toY;             // Target cell coordinates
    MaterialType material;    // Type of material being transferred

    // Collision-specific data
    CollisionType collision_type = CollisionType::TRANSFER_ONLY;
    double collision_energy = 0.0;        // Calculated impact energy
    double restitution_coefficient = 0.0; // Material-specific bounce factor
    double material_mass = 0.0;           // Mass of moving material
    double target_mass = 0.0;             // Mass of target material (if any)

    // Pressure from excess material that can't transfer
    double pressure_from_excess = 0.0; // Pressure to add to target cell
};

} // namespace DirtSim