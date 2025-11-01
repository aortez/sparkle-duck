#pragma once

#include "MaterialType.h"
#include "Vector2d.h"
#include "Vector2i.h"
#include "WorldCalculatorBase.h"
#include <cstdint>
#include <vector>

class Cell;
class World;

/**
 * @brief Calculates contact-based friction forces for World physics.
 *
 * This class implements true surface friction between adjacent cells based on:
 * - Normal force (pressure difference + weight for vertical contacts)
 * - Relative tangential velocity between surfaces
 * - Material-specific static and kinetic friction coefficients
 *
 * Friction forces oppose relative sliding motion between contacting surfaces.
 */
class WorldFrictionCalculator : public WorldCalculatorBase {
public:
    /**
     * @brief Constructor takes a World for accessing world data.
     * @param world World providing access to grid and cells.
     */
    explicit WorldFrictionCalculator(const World& world);

    /**
     * @brief Data structure representing a contact interface between two cells.
     */
    struct ContactInterface {
        Vector2i cell_A_pos;              // Position of first cell.
        Vector2i cell_B_pos;              // Position of second cell.
        Vector2d interface_normal;        // Unit vector pointing from A to B.
        double contact_area;              // Relative contact area (1.0 cardinal, 0.707 diagonal).
        double normal_force;              // Force pressing surfaces together.
        Vector2d relative_velocity;       // Velocity of A relative to B.
        Vector2d tangential_velocity;     // Tangential component of relative velocity.
        double friction_coefficient;      // Combined friction coefficient (static or kinetic).
    };

    /**
     * @brief Calculate and apply friction forces for all contact interfaces.
     * @param deltaTime Time step for physics integration.
     */
    void calculateAndApplyFrictionForces(double deltaTime);

    /**
     * @brief Set the global friction strength multiplier.
     * @param strength Multiplier for all friction forces (0.0 = disabled, 1.0 = normal).
     */
    void setFrictionStrength(double strength) { friction_strength_ = strength; }

    /**
     * @brief Get the global friction strength multiplier.
     * @return Current friction strength.
     */
    double getFrictionStrength() const { return friction_strength_; }

private:
    /**
     * @brief Detect all contact interfaces in the world.
     * @return Vector of contact interfaces with calculated properties.
     */
    std::vector<ContactInterface> detectContactInterfaces() const;

    /**
     * @brief Calculate normal force for a contact interface.
     * @param cellA First cell in contact.
     * @param cellB Second cell in contact.
     * @param posA Position of first cell.
     * @param posB Position of second cell.
     * @param interface_normal Normal vector of interface (A to B).
     * @return Normal force magnitude.
     */
    double calculateNormalForce(
        const Cell& cellA,
        const Cell& cellB,
        const Vector2i& posA,
        const Vector2i& posB,
        const Vector2d& interface_normal) const;

    /**
     * @brief Calculate friction coefficient based on relative tangential velocity.
     * @param tangential_speed Magnitude of tangential relative velocity.
     * @param propsA Material properties of first cell.
     * @param propsB Material properties of second cell.
     * @return Combined friction coefficient.
     */
    double calculateFrictionCoefficient(
        double tangential_speed,
        const MaterialProperties& propsA,
        const MaterialProperties& propsB) const;

    /**
     * @brief Decompose relative velocity into normal and tangential components.
     * @param relative_velocity Velocity of A relative to B.
     * @param interface_normal Normal vector of interface.
     * @return Tangential component of relative velocity.
     */
    Vector2d calculateTangentialVelocity(
        const Vector2d& relative_velocity,
        const Vector2d& interface_normal) const;

    /**
     * @brief Apply friction forces to cells based on contact interfaces.
     * @param contacts Vector of contact interfaces with calculated properties.
     */
    void applyFrictionForces(const std::vector<ContactInterface>& contacts);

    // Configuration parameters.
    double friction_strength_ = 1.0;

    // Physical constants.
    static constexpr double MIN_NORMAL_FORCE = 0.01;  // Minimum normal force for friction.
    static constexpr double MIN_TANGENTIAL_SPEED = 1e-6;  // Minimum speed to apply friction.
};
