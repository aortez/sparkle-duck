#pragma once

#include "MaterialMove.h"
#include "MaterialType.h"
#include "Vector2d.h"
#include "Vector2i.h"
#include "WorldCalculatorBase.h"
#include "WorldCohesionCalculator.h"
#include <vector>

namespace DirtSim {

class Cell;
class World;

/**
 * @brief Calculator for collision detection and response in World
 *
 * This calculator handles all collision-related physics including:
 * - Collision detection between materials
 * - Collision type determination (elastic, inelastic, fragmentation, absorption)
 * - Collision response physics (momentum transfer, energy calculations)
 * - Boundary reflections (world and cell boundaries)
 * - Floating particle collision system
 *
 * The collision system implements material-specific interaction behaviors
 * based on physical properties like density, elasticity, and brittleness.
 */
class WorldCollisionCalculator : public WorldCalculatorBase {
public:
    // Default constructor - calculator is stateless.
    WorldCollisionCalculator() = default;

    // ===== COLLISION DETECTION =====

    /**
     * @brief Detect all boundary crossings for a given COM position.
     * @param newCOM The new center of mass position to check.
     * @return Vector of directions where boundaries are crossed.
     */
    std::vector<Vector2i> getAllBoundaryCrossings(const Vector2d& newCOM) const;

    /**
     * @brief Create a collision-aware material move with physics data.
     * @param world World providing access to grid and cells.
     * @param fromCell Source cell.
     * @param toCell Target cell.
     * @param fromPos Source position.
     * @param toPos Target position.
     * @param direction Movement direction.
     * @param deltaTime Time step.
     * @param com_cohesion COM cohesion force data.
     * @return MaterialMove with collision physics data.
     */
    MaterialMove createCollisionAwareMove(
        const World& world,
        const Cell& fromCell,
        const Cell& toCell,
        const Vector2i& fromPos,
        const Vector2i& toPos,
        const Vector2i& direction,
        double deltaTime,
        const WorldCohesionCalculator::COMCohesionForce& com_cohesion) const;

    /**
     * @brief Determine collision type based on materials and energy.
     * @param from Source material type.
     * @param to Target material type.
     * @param collision_energy Kinetic energy of collision.
     * @return Type of collision that should occur.
     */
    CollisionType determineCollisionType(
        MaterialType from, MaterialType to, double collision_energy) const;

    /**
     * @brief Calculate kinetic energy of a collision.
     * @param move Material move data.
     * @param fromCell Source cell.
     * @param toCell Target cell.
     * @return Collision energy in physics units.
     */
    double calculateCollisionEnergy(
        const MaterialMove& move, const Cell& fromCell, const Cell& toCell) const;

    /**
     * @brief Calculate mass of material in a cell.
     * @param cell Cell to calculate mass for.
     * @return Mass based on material density and fill ratio.
     */
    double calculateMaterialMass(const Cell& cell) const;

    /**
     * @brief Check if floating particle collides with target cell.
     * @param world World providing access to grid and cells.
     * @param cellX Target cell X coordinate.
     * @param cellY Target cell Y coordinate.
     * @param floating_particle The floating particle.
     * @return True if collision occurs.
     */
    bool checkFloatingParticleCollision(
        const World& world, int cellX, int cellY, const Cell& floating_particle) const;

    // ===== COLLISION RESPONSE =====

    /**
     * @brief Handle basic material transfer (no collision).
     * @param world World providing access to grid and cells.
     * @param fromCell Source cell.
     * @param toCell Target cell.
     * @param move Material move data.
     */
    void handleTransferMove(World& world, Cell& fromCell, Cell& toCell, const MaterialMove& move);

    /**
     * @brief Handle elastic collision between materials.
     * @param fromCell Source cell.
     * @param toCell Target cell.
     * @param move Material move data.
     */
    void handleElasticCollision(Cell& fromCell, Cell& toCell, const MaterialMove& move);

    /**
     * @brief Handle inelastic collision with momentum transfer.
     * @param world World providing access to grid and cells.
     * @param fromCell Source cell.
     * @param toCell Target cell.
     * @param move Material move data.
     */
    void handleInelasticCollision(
        World& world, Cell& fromCell, Cell& toCell, const MaterialMove& move);

    /**
     * @brief Handle material fragmentation on high-energy impact.
     * @param fromCell Source cell.
     * @param toCell Target cell.
     * @param move Material move data.
     */
    void handleFragmentation(World& world, Cell& fromCell, Cell& toCell, const MaterialMove& move);

    /**
     * @brief Handle material absorption (e.g., water into dirt).
     * @param fromCell Source cell.
     * @param toCell Target cell.
     * @param move Material move data.
     */
    void handleAbsorption(World& world, Cell& fromCell, Cell& toCell, const MaterialMove& move);

    /**
     * @brief Handle floating particle collision response.
     * @param world World providing access to grid and cells.
     * @param cellX Target cell X coordinate.
     * @param cellY Target cell Y coordinate.
     * @param floating_particle The floating particle.
     * @param targetCell Target cell to modify.
     */
    void handleFloatingParticleCollision(
        World& world, int cellX, int cellY, const Cell& floating_particle, Cell& targetCell);

    // ===== BOUNDARY REFLECTIONS =====

    /**
     * @brief Apply elastic reflection at world boundaries.
     * @param cell Cell to apply reflection to.
     * @param direction Direction of boundary hit.
     */
    void applyBoundaryReflection(Cell& cell, const Vector2i& direction);

    /**
     * @brief Apply reflection when cell-to-cell transfer fails.
     * @param cell Cell to apply reflection to.
     * @param direction Direction of failed transfer.
     * @param material Material type for elasticity calculation.
     */
    void applyCellBoundaryReflection(Cell& cell, const Vector2i& direction, MaterialType material);

    bool shouldSwapMaterials(
        const World& world,
        uint32_t fromX,
        uint32_t fromY,
        const Cell& fromCell,
        const Cell& toCell,
        const Vector2i& direction,
        const MaterialMove& move) const;

    /**
     * @brief Swap materials between two counter-moving cells with energy conservation.
     * Deducts swap cost from moving material's velocity.
     */
    void swapCounterMovingMaterials(
        Cell& fromCell, Cell& toCell, const Vector2i& direction, const MaterialMove& move);

    /**
     * @brief Check if density difference supports swap in the given direction.
     * Returns true if lighter material is moving up or heavier material is moving down.
     */
    bool densitySupportsSwap(
        const Cell& fromCell, const Cell& toCell, const Vector2i& direction) const;

    // ===== UTILITY METHODS =====

    /**
     * @brief Velocity decomposition result for collision physics.
     */
    struct VelocityComponents {
        Vector2d normal;      // Normal component (perpendicular to surface).
        Vector2d tangential;  // Tangential component (parallel to surface).
        double normal_scalar; // Signed magnitude of normal component.
    };

    /**
     * @brief Decompose velocity into normal and tangential components.
     * @param velocity The velocity vector to decompose.
     * @param surface_normal The surface normal (will be normalized internally).
     * @return Components struct with normal, tangential, and normal scalar.
     */
    VelocityComponents decomposeVelocity(
        const Vector2d& velocity, const Vector2d& surface_normal) const;

    /**
     * @brief Check if a material is considered rigid for collision purposes.
     * @param material Material type to check.
     * @return True if material is rigid (METAL, WOOD, WALL, etc.).
     */
    static bool isMaterialRigid(MaterialType material);

    double calculateCohesionStrength(
        const Cell& cell, const World& world, uint32_t x, uint32_t y) const;

private:
    static constexpr double FRAGMENTATION_THRESHOLD = 15.0;
    static constexpr double INELASTIC_RESTITUTION_FACTOR = 0.5;
    static constexpr double COHESION_RESISTANCE_FACTOR = 20.0;
};

} // namespace DirtSim