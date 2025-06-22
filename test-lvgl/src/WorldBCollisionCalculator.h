#pragma once

#include "MaterialMove.h"
#include "MaterialType.h"
#include "Vector2d.h"
#include "Vector2i.h"
#include "WorldBCalculatorBase.h"
#include "WorldBCohesionCalculator.h"
#include <vector>

class CellB;
class WorldB;

/**
 * @brief Calculator for collision detection and response in WorldB
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
class WorldBCollisionCalculator : public WorldBCalculatorBase
{
public:
    /**
     * @brief Constructor
     * @param world Non-const reference since collisions modify world state
     */
    explicit WorldBCollisionCalculator(WorldB& world);

    // ===== COLLISION DETECTION =====

    /**
     * @brief Detect all boundary crossings for a given COM position
     * @param newCOM The new center of mass position to check
     * @return Vector of directions where boundaries are crossed
     */
    std::vector<Vector2i> getAllBoundaryCrossings(const Vector2d& newCOM) const;

    /**
     * @brief Create a collision-aware material move with physics data
     * @param fromCell Source cell
     * @param toCell Target cell
     * @param fromPos Source position
     * @param toPos Target position
     * @param direction Movement direction
     * @param deltaTime Time step
     * @param com_cohesion COM cohesion force data
     * @return MaterialMove with collision physics data
     */
    MaterialMove createCollisionAwareMove(
        const CellB& fromCell,
        const CellB& toCell,
        const Vector2i& fromPos,
        const Vector2i& toPos,
        const Vector2i& direction,
        double deltaTime,
        const WorldBCohesionCalculator::COMCohesionForce& com_cohesion) const;

    /**
     * @brief Determine collision type based on materials and energy
     * @param from Source material type
     * @param to Target material type
     * @param collision_energy Kinetic energy of collision
     * @return Type of collision that should occur
     */
    CollisionType determineCollisionType(
        MaterialType from, MaterialType to, double collision_energy) const;

    /**
     * @brief Calculate kinetic energy of a collision
     * @param move Material move data
     * @param fromCell Source cell
     * @param toCell Target cell
     * @return Collision energy in physics units
     */
    double calculateCollisionEnergy(
        const MaterialMove& move, const CellB& fromCell, const CellB& toCell) const;

    /**
     * @brief Calculate mass of material in a cell
     * @param cell Cell to calculate mass for
     * @return Mass based on material density and fill ratio
     */
    double calculateMaterialMass(const CellB& cell) const;

    /**
     * @brief Check if floating particle collides with target cell
     * @param cellX Target cell X coordinate
     * @param cellY Target cell Y coordinate
     * @param floating_particle The floating particle
     * @return True if collision occurs
     */
    bool checkFloatingParticleCollision(
        int cellX, int cellY, const CellB& floating_particle) const;

    // ===== COLLISION RESPONSE =====

    /**
     * @brief Handle basic material transfer (no collision)
     * @param fromCell Source cell
     * @param toCell Target cell
     * @param move Material move data
     */
    void handleTransferMove(CellB& fromCell, CellB& toCell, const MaterialMove& move);

    /**
     * @brief Handle elastic collision between materials
     * @param fromCell Source cell
     * @param toCell Target cell
     * @param move Material move data
     */
    void handleElasticCollision(CellB& fromCell, CellB& toCell, const MaterialMove& move);

    /**
     * @brief Handle inelastic collision with momentum transfer
     * @param fromCell Source cell
     * @param toCell Target cell
     * @param move Material move data
     */
    void handleInelasticCollision(CellB& fromCell, CellB& toCell, const MaterialMove& move);

    /**
     * @brief Handle material fragmentation on high-energy impact
     * @param fromCell Source cell
     * @param toCell Target cell
     * @param move Material move data
     */
    void handleFragmentation(CellB& fromCell, CellB& toCell, const MaterialMove& move);

    /**
     * @brief Handle material absorption (e.g., water into dirt)
     * @param fromCell Source cell
     * @param toCell Target cell
     * @param move Material move data
     */
    void handleAbsorption(CellB& fromCell, CellB& toCell, const MaterialMove& move);

    /**
     * @brief Handle floating particle collision response
     * @param cellX Target cell X coordinate
     * @param cellY Target cell Y coordinate
     * @param floating_particle The floating particle
     * @param targetCell Target cell to modify
     */
    void handleFloatingParticleCollision(
        int cellX, int cellY, const CellB& floating_particle, CellB& targetCell);

    // ===== BOUNDARY REFLECTIONS =====

    /**
     * @brief Apply elastic reflection at world boundaries
     * @param cell Cell to apply reflection to
     * @param direction Direction of boundary hit
     */
    void applyBoundaryReflection(CellB& cell, const Vector2i& direction);

    /**
     * @brief Apply reflection when cell-to-cell transfer fails
     * @param cell Cell to apply reflection to
     * @param direction Direction of failed transfer
     * @param material Material type for elasticity calculation
     */
    void applyCellBoundaryReflection(
        CellB& cell, const Vector2i& direction, MaterialType material);

    // ===== UTILITY METHODS =====

    /**
     * @brief Check if a material is considered rigid for collision purposes
     * @param material Material type to check
     * @return True if material is rigid (METAL, WOOD, WALL, etc.)
     */
    static bool isMaterialRigid(MaterialType material);

private:
    WorldB& world_; // Non-const reference for modifying world state

    // Physics constants
    static constexpr double FRAGMENTATION_THRESHOLD = 15.0;
    static constexpr double INELASTIC_RESTITUTION_FACTOR = 0.5;
};