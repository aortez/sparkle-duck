#include "WorldCollisionCalculator.h"
#include "Cell.h"
#include "LoggingChannels.h"
#include "MaterialMove.h"
#include "MaterialType.h"
#include "PhysicsSettings.h"
#include "World.h"
#include "WorldCohesionCalculator.h"
#include "WorldData.h"
#include "WorldPressureCalculator.h"
#include "spdlog/spdlog.h"
#include <algorithm>
#include <cmath>

using namespace DirtSim;

// =================================================================
// COLLISION DETECTION.
// =================================================================

BoundaryCrossings WorldCollisionCalculator::getAllBoundaryCrossings(const Vector2d& newCOM) const
{
    BoundaryCrossings crossings;

    // Check each boundary independently (aligned with original shouldTransfer logic).
    if (newCOM.x >= 1.0) crossings.add(Vector2i(1, 0));   // Right boundary.
    if (newCOM.x <= -1.0) crossings.add(Vector2i(-1, 0)); // Left boundary.
    if (newCOM.y >= 1.0) crossings.add(Vector2i(0, 1));   // Down boundary.
    if (newCOM.y <= -1.0) crossings.add(Vector2i(0, -1)); // Up boundary.

    return crossings;
}

MaterialMove WorldCollisionCalculator::createCollisionAwareMove(
    const World& world,
    const Cell& fromCell,
    const Cell& toCell,
    const Vector2i& fromPos,
    const Vector2i& toPos,
    const Vector2i& direction,
    double /* deltaTime */) const
{
    MaterialMove move;

    // Standard move data.
    move.fromX = fromPos.x;
    move.fromY = fromPos.y;
    move.toX = toPos.x;
    move.toY = toPos.y;
    move.material = fromCell.material_type;

    // Calculate how much wants to transfer vs what can transfer.
    double wants_to_transfer = fromCell.fill_ratio; // Cell wants to follow its COM.
    double capacity = toCell.getCapacity();

    // Queue only what will actually succeed.
    move.amount = std::min(wants_to_transfer, capacity);

    // Store pressure generation info in the move for later application.
    double excess = wants_to_transfer - move.amount;
    move.pressure_from_excess = 0.0; // Initialize.

    if (excess > MIN_MATTER_THRESHOLD && world.getPhysicsSettings().pressure_dynamic_strength > 0) {
        double blocked_mass = excess * getMaterialDensity(fromCell.material_type);
        double energy = fromCell.velocity.magnitude() * blocked_mass;
        double dynamic_strength = world.getPhysicsSettings().pressure_dynamic_strength;
        double pressure_increase =
            energy * 0.1 * dynamic_strength; // Apply dynamic pressure strength.

        // Store pressure to be applied to target cell when processing moves.
        move.pressure_from_excess = pressure_increase;

        spdlog::debug(
            "Pressure from excess at ({},{}) -> ({},{}): excess={:.3f}, energy={:.3f}, "
            "dynamic_strength={:.3f}, pressure_to_add={:.3f}",
            fromPos.x,
            fromPos.y,
            toPos.x,
            toPos.y,
            excess,
            energy,
            dynamic_strength,
            pressure_increase);
    }

    move.momentum = fromCell.velocity;
    move.boundary_normal =
        Vector2d{ static_cast<double>(direction.x), static_cast<double>(direction.y) };

    // Calculate collision physics data.
    move.material_mass = calculateMaterialMass(fromCell);
    move.target_mass = calculateMaterialMass(toCell);
    move.collision_energy = calculateCollisionEnergy(move, fromCell, toCell);

    // Determine collision type based on materials and energy.
    move.collision_type =
        determineCollisionType(fromCell.material_type, toCell.material_type, move.collision_energy);

    // Set material-specific restitution coefficient.
    const auto& fromProps = fromCell.material();
    const auto& toProps = toCell.material();

    if (move.collision_type == CollisionType::ELASTIC_REFLECTION) {
        // For elastic collisions, use geometric mean of elasticities.
        move.restitution_coefficient = std::sqrt(fromProps.elasticity * toProps.elasticity);
    }
    else if (move.collision_type == CollisionType::INELASTIC_COLLISION) {
        // For inelastic collisions, reduce restitution significantly.
        move.restitution_coefficient = std::sqrt(fromProps.elasticity * toProps.elasticity) * 0.3;
    }
    else if (move.collision_type == CollisionType::FRAGMENTATION) {
        // Fragmentation has very low restitution.
        move.restitution_coefficient = 0.1;
    }
    else {
        // Transfer and absorption have minimal bounce.
        move.restitution_coefficient = 0.0;
    }

    return move;
}

CollisionType WorldCollisionCalculator::determineCollisionType(
    MaterialType from, MaterialType to, double collision_energy) const
{
    // Get material properties for both materials.
    const auto& fromProps = getMaterialProperties(from);
    const auto& toProps = getMaterialProperties(to);

    // Empty cells allow transfer.
    if (to == MaterialType::AIR) {
        return CollisionType::TRANSFER_ONLY;
    }

    // High-energy impacts on brittle materials cause fragmentation.
    if (collision_energy > FRAGMENTATION_THRESHOLD
        && (from == MaterialType::WOOD || from == MaterialType::LEAF)
        && (to == MaterialType::METAL || to == MaterialType::WALL)) {
        return CollisionType::FRAGMENTATION;
    }

    // Material-specific interaction matrix.

    // METAL interactions - highly elastic due to high elasticity (0.8)
    if (from == MaterialType::METAL || to == MaterialType::METAL) {
        if (to == MaterialType::WALL || from == MaterialType::WALL) {
            return CollisionType::ELASTIC_REFLECTION; // Metal vs wall.
        }
        if ((from == MaterialType::METAL && to == MaterialType::METAL)
            || (from == MaterialType::METAL && isMaterialRigid(to))
            || (to == MaterialType::METAL && isMaterialRigid(from))) {
            return CollisionType::ELASTIC_REFLECTION; // Metal vs rigid materials.
        }
        return CollisionType::INELASTIC_COLLISION; // Metal vs soft materials.
    }

    // WALL interactions - always elastic due to infinite mass.
    if (to == MaterialType::WALL) {
        return CollisionType::ELASTIC_REFLECTION;
    }

    // WOOD interactions - moderately elastic (0.6 elasticity)
    if (from == MaterialType::WOOD && isMaterialRigid(to)) {
        return CollisionType::ELASTIC_REFLECTION;
    }

    // AIR interactions - highly elastic (1.0 elasticity) but low mass.
    if (from == MaterialType::AIR) {
        return CollisionType::ELASTIC_REFLECTION;
    }

    // Rigid-to-rigid collisions based on elasticity.
    if (isMaterialRigid(from) && isMaterialRigid(to)) {
        double avg_elasticity = (fromProps.elasticity + toProps.elasticity) / 2.0;
        return (avg_elasticity > 0.5) ? CollisionType::ELASTIC_REFLECTION
                                      : CollisionType::INELASTIC_COLLISION;
    }

    // Fluid absorption behaviors.
    if ((from == MaterialType::WATER && to == MaterialType::DIRT)
        || (from == MaterialType::DIRT && to == MaterialType::WATER)) {
        return CollisionType::ABSORPTION;
    }

    // Dense materials hitting lighter materials.
    if (fromProps.density > toProps.density * 2.0) {
        return CollisionType::INELASTIC_COLLISION; // Heavy impacts soft.
    }

    // Default: inelastic collision for general material interactions.
    return CollisionType::INELASTIC_COLLISION;
}

double WorldCollisionCalculator::calculateCollisionEnergy(
    const MaterialMove& move, const Cell& fromCell, const Cell& toCell) const
{
    // Kinetic energy: KE = 0.5 × m × v²
    // Use FULL cell mass for collision energy, not just transferable amount.
    // This is needed for swap decisions when target cell is full (move.amount = 0).
    double movingMass = calculateMaterialMass(fromCell);

    // IMPORTANT: Use velocity component in direction of movement, not total magnitude.
    // For swaps, only energy in the swap direction matters.
    // If falling vertically with little horizontal velocity, horizontal swaps should be hard.
    Vector2d direction_vector(move.toX - move.fromX, move.toY - move.fromY);
    double velocity_in_direction = std::abs(move.momentum.dot(direction_vector));

    LoggingChannels::swap()->debug(
        "Energy calc: total_vel=({:.3f},{:.3f}), dir=({},{}), vel_in_dir={:.3f}",
        move.momentum.x,
        move.momentum.y,
        move.toX - move.fromX,
        move.toY - move.fromY,
        velocity_in_direction);

    // If target cell has material, include reduced mass for collision.
    double targetMass = calculateMaterialMass(toCell);
    double effective_mass = movingMass;

    if (targetMass > 0.0) {
        // Reduced mass formula: μ = (m1 × m2) / (m1 + m2)
        effective_mass = (movingMass * targetMass) / (movingMass + targetMass);
    }

    return 0.5 * effective_mass * velocity_in_direction * velocity_in_direction;
}

double WorldCollisionCalculator::calculateMaterialMass(const Cell& cell) const
{
    if (cell.isEmpty()) return 0.0;

    // Mass = density × volume.
    // Volume = fill_ratio (since cell volume is normalized to 1.0)
    double density = getMaterialDensity(cell.material_type);
    double volume = cell.fill_ratio;
    return density * volume;
}

bool WorldCollisionCalculator::checkFloatingParticleCollision(
    const World& world, int cellX, int cellY, const Cell& floating_particle) const
{
    if (!isValidCell(world, cellX, cellY)) {
        return false;
    }

    const Cell& targetCell = getCellAt(world, cellX, cellY);

    // Check if there's material to collide with.
    if (!targetCell.isEmpty()) {
        // Get material properties for collision behavior.
        const MaterialProperties& floatingProps =
            getMaterialProperties(floating_particle.material_type);
        const MaterialProperties& targetProps = getMaterialProperties(targetCell.material_type);

        // For now, simple collision detection - can be enhanced later.
        // Heavy materials (like METAL) can push through lighter materials.
        // Solid materials (like WALL) stop everything.
        if (targetCell.material_type == MaterialType::WALL) {
            return true; // Wall stops everything.
        }

        // Check density-based collision.
        if (floatingProps.density <= targetProps.density) {
            return true; // Can't push through denser material.
        }
    }

    return false;
}

// =================================================================
// COLLISION RESPONSE.
// =================================================================

void WorldCollisionCalculator::handleTransferMove(
    World& world, Cell& fromCell, Cell& toCell, const MaterialMove& move)
{
    // Log pre-transfer state.
    spdlog::debug(
        "TRANSFER: Before - From({},{}) vel=({:.3f},{:.3f}) fill={:.3f}, To({},{}) "
        "vel=({:.3f},{:.3f}) fill={:.3f}",
        move.fromX,
        move.fromY,
        fromCell.velocity.x,
        fromCell.velocity.y,
        fromCell.fill_ratio,
        move.toX,
        move.toY,
        toCell.velocity.x,
        toCell.velocity.y,
        toCell.fill_ratio);

    // Attempt the transfer.
    const double transferred =
        fromCell.transferToWithPhysics(toCell, move.amount, move.boundary_normal);

    // Log post-transfer state.
    spdlog::debug(
        "TRANSFER: After  - From({},{}) vel=({:.3f},{:.3f}) fill={:.3f}, To({},{}) "
        "vel=({:.3f},{:.3f}) fill={:.3f}",
        move.fromX,
        move.fromY,
        fromCell.velocity.x,
        fromCell.velocity.y,
        fromCell.fill_ratio,
        move.toX,
        move.toY,
        toCell.velocity.x,
        toCell.velocity.y,
        toCell.fill_ratio);

    if (transferred > 0.0) {
        spdlog::trace(
            "Transferred {:.3f} {} from ({},{}) to ({},{}) with boundary normal ({:.2f},{:.2f})",
            transferred,
            getMaterialName(move.material),
            move.fromX,
            move.fromY,
            move.toX,
            move.toY,
            move.boundary_normal.x,
            move.boundary_normal.y);
    }

    // Check if transfer was incomplete (target full or couldn't accept all material)
    const double transfer_deficit = move.amount - transferred;
    if (transfer_deficit > MIN_MATTER_THRESHOLD) {
        // Transfer failed partially or completely - apply elastic reflection for remaining
        // material.
        Vector2i direction(move.toX - move.fromX, move.toY - move.fromY);

        spdlog::debug(
            "Transfer incomplete: requested={:.3f}, transferred={:.3f}, deficit={:.3f} - applying "
            "reflection",
            move.amount,
            transferred,
            transfer_deficit);

        // Queue blocked transfer for dynamic pressure accumulation.
        if (world.getPhysicsSettings().pressure_dynamic_strength > 0) {
            // Calculate energy with proper mass consideration.
            double material_density = getMaterialDensity(move.material);
            double blocked_mass = transfer_deficit * material_density;
            double energy = fromCell.velocity.magnitude() * blocked_mass;

            spdlog::debug(
                "Blocked transfer energy calculation: material={}, density={:.2f}, "
                "blocked_mass={:.4f}, velocity={:.2f}, energy={:.4f}",
                getMaterialName(move.material),
                material_density,
                blocked_mass,
                fromCell.velocity.magnitude(),
                energy);

            world.getPressureCalculator().queueBlockedTransfer(
                { move.fromX,
                  move.fromY,
                  move.toX,
                  move.toY,
                  transfer_deficit, // transfer_amount.
                  fromCell.velocity,
                  energy });
        }

        applyCellBoundaryReflection(fromCell, direction, move.material);
    }
}

void WorldCollisionCalculator::handleElasticCollision(
    Cell& fromCell, Cell& toCell, const MaterialMove& move)
{
    Vector2d incident_velocity = move.momentum;
    Vector2d surface_normal = move.boundary_normal.normalize();

    if (move.target_mass > 0.0 && !toCell.isEmpty()) {
        // Two-body elastic collision with proper normal/tangential decomposition.
        Vector2d target_velocity = toCell.velocity;
        double m1 = move.material_mass;
        double m2 = move.target_mass;

        // Decompose both velocities into normal and tangential components.
        auto v1_comp = decomposeVelocity(incident_velocity, surface_normal);
        auto v2_comp = decomposeVelocity(target_velocity, surface_normal);

        // Apply 1D elastic collision formulas ONLY to normal components.
        // v1_normal' = ((m1-m2)*v1_normal + 2*m2*v2_normal)/(m1+m2)
        // v2_normal' = ((m2-m1)*v2_normal + 2*m1*v1_normal)/(m1+m2)
        double v1_normal_new_scalar =
            ((m1 - m2) * v1_comp.normal_scalar + 2.0 * m2 * v2_comp.normal_scalar) / (m1 + m2);
        double v2_normal_new_scalar =
            ((m2 - m1) * v2_comp.normal_scalar + 2.0 * m1 * v1_comp.normal_scalar) / (m1 + m2);

        // Apply restitution coefficient ONLY to normal components.
        v1_normal_new_scalar *= move.restitution_coefficient;
        v2_normal_new_scalar *= move.restitution_coefficient;

        // Recombine: final velocity = tangential (preserved) + normal (modified).
        Vector2d new_v1 = v1_comp.tangential + surface_normal * v1_normal_new_scalar;
        Vector2d new_v2 = v2_comp.tangential + surface_normal * v2_normal_new_scalar;

        fromCell.velocity = new_v1;
        toCell.velocity = new_v2;

        // Separate particles to prevent repeated collisions.
        // Move the particle that crossed the boundary back slightly.
        double separation_distance = 0.02; // Small separation to ensure clean separation.
        Vector2d fromCOM = fromCell.com;

        // Check which boundary was crossed and apply separation.
        if (move.boundary_normal.x > 0.5) { // Crossed right boundary (normal points left)
            fromCOM.x = std::min(fromCOM.x, 1.0 - separation_distance);
            fromCell.setCOM(fromCOM);
        }
        else if (move.boundary_normal.x < -0.5) { // Crossed left boundary (normal points right)
            fromCOM.x = std::max(fromCOM.x, -1.0 + separation_distance);
            fromCell.setCOM(fromCOM);
        }

        if (move.boundary_normal.y > 0.5) { // Crossed bottom boundary (normal points up)
            fromCOM.y = std::min(fromCOM.y, 1.0 - separation_distance);
            fromCell.setCOM(fromCOM);
        }
        else if (move.boundary_normal.y < -0.5) { // Crossed top boundary (normal points down)
            fromCOM.y = std::max(fromCOM.y, -1.0 + separation_distance);
            fromCell.setCOM(fromCOM);
        }

        spdlog::trace(
            "Elastic collision: {} vs {} at ({},{}) -> ({},{}) - masses: {:.2f}, {:.2f}, "
            "restitution: {:.2f}, COM adjusted to ({:.3f},{:.3f})",
            getMaterialName(move.material),
            getMaterialName(toCell.material_type),
            move.fromX,
            move.fromY,
            move.toX,
            move.toY,
            m1,
            m2,
            move.restitution_coefficient,
            fromCOM.x,
            fromCOM.y);
    }
    else {
        // Empty target or zero mass - reflect off surface with proper decomposition.
        auto v_comp = decomposeVelocity(incident_velocity, surface_normal);

        // Apply restitution only to normal component, preserve tangential.
        Vector2d v_normal_reflected = v_comp.normal * (-move.restitution_coefficient);
        Vector2d reflected_velocity = v_comp.tangential + v_normal_reflected;

        fromCell.velocity = reflected_velocity;

        // Also apply separation for reflections.
        double separation_distance = 0.02;
        Vector2d fromCOM = fromCell.com;

        if (surface_normal.x > 0.5) {
            fromCOM.x = std::min(fromCOM.x, 1.0 - separation_distance);
        }
        else if (surface_normal.x < -0.5) {
            fromCOM.x = std::max(fromCOM.x, -1.0 + separation_distance);
        }

        if (surface_normal.y > 0.5) {
            fromCOM.y = std::min(fromCOM.y, 1.0 - separation_distance);
        }
        else if (surface_normal.y < -0.5) {
            fromCOM.y = std::max(fromCOM.y, -1.0 + separation_distance);
        }

        fromCell.setCOM(fromCOM);
    }

    // Minimal or no material transfer for elastic collisions.
    // Material stays in original cell with new velocity.
}

void WorldCollisionCalculator::handleInelasticCollision(
    World& world, Cell& fromCell, Cell& toCell, const MaterialMove& move)
{
    // Physics-correct component-based collision handling.
    Vector2d incident_velocity = move.momentum;
    Vector2d surface_normal = move.boundary_normal.normalize();

    // Decompose velocity into normal and tangential components.
    auto v_comp = decomposeVelocity(incident_velocity, surface_normal);

    // Apply restitution only to normal component, preserve tangential.
    double inelastic_restitution = move.restitution_coefficient * INELASTIC_RESTITUTION_FACTOR;
    Vector2d v_normal_reflected = v_comp.normal * (-inelastic_restitution);
    Vector2d final_velocity = v_comp.tangential + v_normal_reflected;

    // Apply the corrected velocity to the incident particle.
    fromCell.velocity = final_velocity;

    // Transfer momentum to target cell (Newton's 3rd law).
    // Even if material transfer fails, momentum must be conserved.
    if (move.target_mass > 0.0) {
        Vector2d momentum_transferred =
            v_comp.normal * (1.0 + inelastic_restitution) * move.material_mass;
        Vector2d target_velocity_change = momentum_transferred / move.target_mass;
        toCell.velocity = toCell.velocity + target_velocity_change;

        spdlog::debug(
            "Momentum transfer: normal=({:.3f},{:.3f}) momentum=({:.3f},{:.3f}) "
            "target_vel_change=({:.3f},{:.3f})",
            v_comp.normal.x,
            v_comp.normal.y,
            momentum_transferred.x,
            momentum_transferred.y,
            target_velocity_change.x,
            target_velocity_change.y);
    }

    // Allow material transfer based on natural capacity limits.
    double transfer_amount = move.amount; // Full amount, let capacity decide.

    // Attempt direct material transfer and measure actual amount transferred.
    double actual_transfer =
        fromCell.transferToWithPhysics(toCell, transfer_amount, move.boundary_normal);

    // Check for blocked transfer and queue for dynamic pressure accumulation.
    double transfer_deficit = transfer_amount - actual_transfer;

    // Debug logging to understand what's happening.
    spdlog::debug(
        "Inelastic collision transfer attempt: requested_amount={:.6f}, "
        "actual_transfer={:.6f}, deficit={:.6f}",
        transfer_amount,
        actual_transfer,
        transfer_deficit);
    spdlog::debug(
        "Dynamic pressure enabled: {}, deficit > threshold: {} (threshold={:.6f})",
        world.getPhysicsSettings().pressure_dynamic_strength > 0,
        transfer_deficit > MIN_MATTER_THRESHOLD,
        MIN_MATTER_THRESHOLD);

    if (transfer_deficit > MIN_MATTER_THRESHOLD
        && world.getPhysicsSettings().pressure_dynamic_strength > 0) {
        spdlog::debug(
            "🚫 Inelastic collision blocked transfer: requested={:.3f}, transferred={:.3f}, "
            "deficit={:.3f}",
            transfer_amount,
            actual_transfer,
            transfer_deficit);

        // Queue blocked transfer for dynamic pressure accumulation.
        // Calculate energy with proper mass consideration.
        double material_density = getMaterialDensity(move.material);
        double blocked_mass = transfer_deficit * material_density;
        double energy = fromCell.velocity.magnitude() * blocked_mass;

        spdlog::debug(
            "Inelastic collision blocked energy: material={}, density={:.2f}, "
            "blocked_mass={:.4f}, velocity={:.2f}, energy={:.4f}",
            getMaterialName(move.material),
            material_density,
            blocked_mass,
            fromCell.velocity.magnitude(),
            energy);

        world.getPressureCalculator().queueBlockedTransfer({ move.fromX,
                                                             move.fromY,
                                                             move.toX,
                                                             move.toY,
                                                             transfer_deficit,
                                                             fromCell.velocity,
                                                             energy });
    }

    spdlog::trace(
        "Inelastic collision: {} at ({},{}) with material transfer {:.3f}, momentum conserved",
        getMaterialName(move.material),
        move.fromX,
        move.fromY,
        actual_transfer);
}

void WorldCollisionCalculator::handleFragmentation(
    World& world, Cell& fromCell, Cell& toCell, const MaterialMove& move)
{
    // TODO: Implement fragmentation mechanics.
    // For now, treat as inelastic collision with complete material transfer.
    spdlog::debug(
        "Fragmentation collision: {} at ({},{}) - treating as inelastic for now",
        getMaterialName(move.material),
        move.fromX,
        move.fromY);

    handleInelasticCollision(world, fromCell, toCell, move);
}

void WorldCollisionCalculator::handleAbsorption(
    World& world, Cell& fromCell, Cell& toCell, const MaterialMove& move)
{
    // One material absorbs the other - implement absorption logic.
    if (move.material == MaterialType::WATER && toCell.material_type == MaterialType::DIRT) {
        // Water absorbed by dirt - transfer all water.
        handleTransferMove(world, fromCell, toCell, move);
        spdlog::trace("Absorption: WATER absorbed by DIRT at ({},{})", move.toX, move.toY);
    }
    else if (move.material == MaterialType::DIRT && toCell.material_type == MaterialType::WATER) {
        // Dirt falls into water - mix materials.
        handleTransferMove(world, fromCell, toCell, move);
        spdlog::trace("Absorption: DIRT mixed with WATER at ({},{})", move.toX, move.toY);
    }
    else {
        // Default to regular transfer.
        handleTransferMove(world, fromCell, toCell, move);
    }
}

void WorldCollisionCalculator::handleFloatingParticleCollision(
    World& world, int cellX, int cellY, const Cell& floating_particle, Cell& targetCell)
{
    (void)world; // Unused for now.
    Vector2d particleVelocity = floating_particle.velocity;

    spdlog::info(
        "Floating particle {} collided with {} at cell ({},{}) with velocity ({:.2f},{:.2f})",
        getMaterialName(floating_particle.material_type),
        getMaterialName(targetCell.material_type),
        cellX,
        cellY,
        particleVelocity.x,
        particleVelocity.y);

    // TODO: Implement collision response based on material properties.
    // - Elastic collisions for METAL vs METAL.
    // - Splash effects for WATER collisions.
    // - Fragmentation for brittle materials.
    // - Momentum transfer based on mass ratios.

    // For now, simple momentum transfer.
    Vector2d currentVelocity = targetCell.velocity;
    double floatingMass = floating_particle.getMass();
    double targetMass = targetCell.getMass();

    if (targetMass > MIN_MATTER_THRESHOLD) {
        // Inelastic collision with momentum conservation.
        Vector2d combinedMomentum = particleVelocity * floatingMass + currentVelocity * targetMass;
        Vector2d newVelocity = combinedMomentum / (floatingMass + targetMass);
        targetCell.velocity = newVelocity;

        spdlog::debug(
            "Applied collision momentum: new velocity ({:.2f},{:.2f})",
            newVelocity.x,
            newVelocity.y);
    }
}

// =================================================================
// BOUNDARY REFLECTIONS.
// =================================================================

void WorldCollisionCalculator::applyBoundaryReflection(Cell& cell, const Vector2i& direction)
{
    Vector2d velocity = cell.velocity;
    Vector2d com = cell.com;
    double elasticity = getMaterialProperties(cell.material_type).elasticity;

    spdlog::debug(
        "Applying boundary reflection: material={} direction=({},{}) elasticity={:.2f} "
        "velocity=({:.2f},{:.2f})",
        getMaterialName(cell.material_type),
        direction.x,
        direction.y,
        elasticity,
        velocity.x,
        velocity.y);

    // Apply elastic reflection for the component perpendicular to the boundary.
    if (direction.x != 0) { // Horizontal boundary (left/right walls)
        velocity.x = -velocity.x * elasticity;
        // Move COM away from boundary to prevent re-triggering boundary detection.
        com.x = (direction.x > 0) ? 0.99 : -0.99;
    }

    if (direction.y != 0) { // Vertical boundary (top/bottom walls)
        velocity.y = -velocity.y * elasticity;
        // Move COM away from boundary to prevent re-triggering boundary detection.
        com.y = (direction.y > 0) ? 0.99 : -0.99;
    }

    cell.velocity = velocity;
    cell.setCOM(com);

    spdlog::debug(
        "Boundary reflection complete: new_velocity=({:.2f},{:.2f}) new_com=({:.2f},{:.2f})",
        velocity.x,
        velocity.y,
        com.x,
        com.y);
}

void WorldCollisionCalculator::applyCellBoundaryReflection(
    Cell& cell, const Vector2i& direction, MaterialType material)
{
    Vector2d velocity = cell.velocity;
    Vector2d com = cell.com;
    double elasticity = getMaterialProperties(material).elasticity;

    spdlog::debug(
        "Applying cell boundary reflection: material={} direction=({},{}) elasticity={:.2f}",
        getMaterialName(material),
        direction.x,
        direction.y,
        elasticity);

    // Apply elastic reflection when transfer between cells fails.
    if (direction.x != 0) { // Horizontal transfer failed.
        velocity.x = -velocity.x * elasticity;
        // Move COM away from the boundary that caused the failed transfer.
        com.x = (direction.x > 0) ? 0.99 : -0.99;
    }

    if (direction.y != 0) { // Vertical transfer failed.
        velocity.y = -velocity.y * elasticity;
        // Move COM away from the boundary that caused the failed transfer.
        com.y = (direction.y > 0) ? 0.99 : -0.99;
    }

    cell.velocity = velocity;
    cell.setCOM(com);

    spdlog::debug(
        "Cell boundary reflection complete: new_velocity=({:.2f},{:.2f}) new_com=({:.2f},{:.2f})",
        velocity.x,
        velocity.y,
        com.x,
        com.y);
}

bool WorldCollisionCalculator::densitySupportsSwap(
    const Cell& fromCell, const Cell& toCell, const Vector2i& direction) const
{
    const double from_density = getMaterialProperties(fromCell.material_type).density;
    const double to_density = getMaterialProperties(toCell.material_type).density;

    if (direction.y > 0) {
        // Moving downward: heavier material should sink.
        return from_density > to_density;
    }
    else {
        // Moving upward: lighter material should rise.
        return from_density < to_density;
    }
}

bool WorldCollisionCalculator::shouldSwapMaterials(
    const World& world,
    uint32_t fromX,
    uint32_t fromY,
    const Cell& fromCell,
    const Cell& toCell,
    const Vector2i& direction,
    const MaterialMove& move) const
{
    if (fromCell.material_type == toCell.material_type) {
        LoggingChannels::swap()->debug("Swap denied: same material type");
        return false;
    }

    // Check if target is rigid AND supported.
    // Unsupported rigid materials (floating in water) can be displaced by buoyancy.
    // Supported rigid materials (resting on ground) cannot be displaced.
    const MaterialProperties& to_props = getMaterialProperties(toCell.material_type);
    if (to_props.is_rigid && toCell.has_any_support) {
        LoggingChannels::swap()->debug(
            "Swap denied: cannot displace supported rigid material {}",
            getMaterialName(toCell.material_type));
        return false;
    }

    // PATH OF LEAST RESISTANCE CHECK.
    // When a vertical swap would displace a fluid (but not AIR), check if that
    // fluid has easier lateral escape routes. If so, deny the swap and let
    // pressure push the fluid sideways instead. This prevents the "cliff climbing"
    // effect where dirt drops through water, pushing water up through solid.
    // AIR is excluded because we want air pockets to fill in naturally.
    const MaterialProperties& from_props = getMaterialProperties(fromCell.material_type);
    if (direction.y != 0 && to_props.is_fluid && toCell.material_type != MaterialType::AIR) {
        const WorldData& data = world.getData();
        const uint32_t toX = fromX + direction.x;
        const uint32_t toY = fromY + direction.y;

        for (int dx : { -1, 1 }) {
            int nx = static_cast<int>(toX) + dx;
            if (nx < 0 || nx >= static_cast<int>(data.width)) {
                continue;
            }

            const Cell& lateral = data.at(nx, toY);

            // If the fluid being displaced has empty space beside it, deny swap.
            // The fluid should escape sideways via pressure, not be pushed vertically.
            if (lateral.isEmpty()) {
                LoggingChannels::swap()->info(
                    "Swap denied (path of least resistance): "
                    "{} at ({},{}) can escape to empty lateral at ({},{})",
                    getMaterialName(toCell.material_type),
                    toX,
                    toY,
                    nx,
                    toY);
                return false;
            }

            // Lower pressure laterally means easier escape for the displaced fluid.
            double lateral_pressure = lateral.pressure;
            double target_pressure = toCell.pressure;
            if (lateral_pressure < target_pressure * 0.5) {
                LoggingChannels::swap()->info(
                    "Swap denied (path of least resistance): "
                    "{} at ({},{}) can escape to lower pressure ({:.2f} vs {:.2f}) at ({},{})",
                    getMaterialName(toCell.material_type),
                    toX,
                    toY,
                    lateral_pressure,
                    target_pressure,
                    nx,
                    toY);
                return false;
            }
        }
    }

    // Check swap requirements based on direction.
    const PhysicsSettings& settings = world.getPhysicsSettings();
    if (direction.y == 0) {
        // Horizontal swap: momentum-based displacement.
        // FROM cell needs enough momentum to push TO cell out of the way.
        double from_mass = from_props.density * fromCell.fill_ratio;
        double from_velocity = std::abs(fromCell.velocity.x);
        double from_momentum = from_mass * from_velocity;

        // Fluids pushing solids sideways is harder - they flow around instead.
        if (from_props.is_fluid && !to_props.is_fluid) {
            from_momentum *= settings.horizontal_non_fluid_penalty;
        }

        // TO: resistance to being displaced.
        double to_mass = to_props.density * toCell.fill_ratio;

        // Cohesion makes materials stick together (dirt > sand).
        double cohesion_resistance = 1.0 + to_props.cohesion;

        // Supported materials are much harder to displace.
        double support_factor =
            toCell.has_any_support ? settings.horizontal_non_fluid_target_resistance : 1.0;

        // Fluids are easier to displace than solids.
        double fluid_factor = 1; // to_props.is_fluid ? 0.2 : 1.0;

        double to_resistance = to_mass * cohesion_resistance * support_factor * fluid_factor;

        // Swap if momentum overcomes resistance.
        const double threshold = settings.horizontal_flow_resistance_factor;
        const bool swap_ok = from_momentum > to_resistance * threshold;

        if (!swap_ok) {
            return false;
        }
        // Log horizontal swap approval details.
        if (toCell.material_type != MaterialType::AIR) {
            // LoggingChannels::swap()->warn(
            //     "Horizontal swap OK: {} -> {} at ({},{}) -> ({},{}) | momentum: {:.3f} (mass: "
            //     "{:.3f}, "
            //     "vel: {:.3f}) | resistance: {:.3f} (mass: {:.3f}, cohesion: {:.3f}, support: "
            //     "{:.1f}, "
            //     "fluid: {:.1f}) | threshold: {:.3f}",
            //     getMaterialName(fromCell.material_type),
            //     getMaterialName(toCell.material_type),
            //     fromX,
            //     fromY,
            //     fromX + direction.x,
            //     fromY + direction.y,
            //     from_momentum,
            //     from_mass,
            //     from_velocity,
            //     to_resistance,
            //     to_mass,
            //     to_props.cohesion,
            //     support_factor,
            //     fluid_factor,
            //     to_resistance * threshold);
        }
    }
    else {
        // Vertical swap: momentum-based with buoyancy assist.
        // Density must support the swap direction AND momentum must overcome resistance.
        const double from_density = from_props.density;
        const double to_density = to_props.density;
        const bool density_ok = densitySupportsSwap(fromCell, toCell, direction);

        if (!density_ok) {
            return false;
        }

        // FROM: momentum in direction of movement.
        double from_mass = from_props.density * fromCell.fill_ratio;
        double from_velocity = std::abs(fromCell.velocity.y);
        double from_momentum = from_mass * from_velocity;

        // Buoyancy adds "free" momentum based on density difference.
        // Larger density differences create stronger buoyancy forces.
        double density_diff = std::abs(from_density - to_density);
        double buoyancy_boost = density_diff * world.getPhysicsSettings().buoyancy_energy_scale;
        double effective_momentum = from_momentum + buoyancy_boost;

        // TO: resistance to being displaced.
        // For vertical swaps, no fluid_factor - must move the mass regardless of fluidity.
        double to_mass = to_props.density * toCell.fill_ratio;
        double cohesion_resistance = 1.0 + to_props.cohesion;
        double support_factor = toCell.has_any_support ? 5.0 : 1.0;
        double to_resistance = to_mass * cohesion_resistance * support_factor;

        // Swap if effective momentum overcomes resistance.
        const double threshold = world.getPhysicsSettings().horizontal_flow_resistance_factor;
        const bool swap_ok = effective_momentum > to_resistance * threshold;

        if (!swap_ok) {
            return false;
        }
        // Log vertical swap approval details.
        if (toCell.material_type != MaterialType::AIR) {
            // LoggingChannels::swap()->warn(
            //     "Vertical swap OK: {} -> {} at ({},{}) -> ({},{}) | momentum: {:.3f} (mass: "
            //     "{:.3f}, "
            //     "vel: {:.3f}, buoyancy: {:.3f}) | resistance: {:.3f} (mass: {:.3f}, cohesion: "
            //     "{:.3f}, "
            //     "support: {:.1f}) | threshold: {:.3f} | dir.y: {} ({})",
            //     getMaterialName(fromCell.material_type),
            //     getMaterialName(toCell.material_type),
            //     fromX,
            //     fromY,
            //     fromX + direction.x,
            //     fromY + direction.y,
            //     effective_momentum,
            //     from_mass,
            //     from_velocity,
            //     buoyancy_boost,
            //     to_resistance,
            //     to_mass,
            //     to_props.cohesion,
            //     support_factor,
            //     to_resistance * threshold,
            //     direction.y,
            //     direction.y > 0 ? "DOWN" : "UP");
        }
    }

    // Check cohesion resistance.
    double cohesion_strength = calculateCohesionStrength(fromCell, world, fromX, fromY);
    double bond_breaking_cost =
        cohesion_strength * world.getPhysicsSettings().cohesion_resistance_factor;

    // Reduce bond cost for fluid interactions (fluids help separate materials).
    if (from_props.is_fluid || to_props.is_fluid) {
        bond_breaking_cost *= world.getPhysicsSettings().fluid_lubrication_factor;
    }

    if (cohesion_strength > 0.01) {
        LoggingChannels::swap()->debug(
            "Cohesion check: {} at ({},{}) | strength: {:.3f}, bond_cost: {:.3f} (fluid_adjusted)",
            getMaterialName(fromCell.material_type),
            fromX,
            fromY,
            cohesion_strength,
            bond_breaking_cost);
    }

    // Calculate swap cost: energy to accelerate target cell's contents to 1 cell/second.
    const double target_mass = toCell.getEffectiveDensity();
    const double SWAP_COST_SCALAR = 1;
    double swap_cost = SWAP_COST_SCALAR * 0.5 * target_mass * 1.0; // KE = 0.5 * m * v^2, v = 1.0

    // Non-fluids require more energy to displace (both source and target).
    if (!from_props.is_fluid || !to_props.is_fluid) {
        swap_cost *= world.getPhysicsSettings().non_fluid_energy_multiplier;
    }

    // Total cost includes base swap cost + bond breaking cost.
    const double total_cost = swap_cost + bond_breaking_cost;
    double available_energy = move.collision_energy;

    // Add buoyancy energy for vertical swaps driven by density differences.
    // Light materials rising or heavy materials sinking get "free" energy from buoyancy.
    if (direction.y != 0) {
        const double density_diff = std::abs(from_props.density - to_props.density);
        const bool is_buoyancy_driven = densitySupportsSwap(fromCell, toCell, direction);

        if (is_buoyancy_driven && density_diff > 0.1) {
            const double buoyancy_energy =
                density_diff * world.getPhysicsSettings().buoyancy_energy_scale;
            available_energy += buoyancy_energy;

            LoggingChannels::swap()->debug(
                "Buoyancy boost: {} <-> {} | density_diff: {:.3f}, buoyancy_energy: {:.3f}, total: "
                "{:.3f}",
                getMaterialName(fromCell.material_type),
                getMaterialName(toCell.material_type),
                density_diff,
                buoyancy_energy,
                available_energy);
        }
    }

    if (available_energy < total_cost) {
        if (bond_breaking_cost > 0.01) {
            LoggingChannels::swap()->debug(
                "Swap denied: insufficient energy to break cohesive bonds ({:.3f} < {:.3f}, "
                "bond_cost: {:.3f})",
                available_energy,
                total_cost,
                bond_breaking_cost);
        }
        else {
            LoggingChannels::swap()->debug(
                "Swap denied: insufficient energy ({:.3f} < {:.3f})", available_energy, total_cost);
        }
        return false;
    }

    if (toCell.material_type == MaterialType::AIR) {
        LoggingChannels::swap()->debug(
            "Swap approved: {} -> {} at ({},{}) -> ({},{}) | Energy: {:.3f} >= {:.3f} (base: "
            "{:.3f}, bonds: {:.3f}) | Dir: ({},{}) {}",
            getMaterialName(fromCell.material_type),
            getMaterialName(toCell.material_type),
            fromX,
            fromY,
            fromX + direction.x,
            fromY + direction.y,
            available_energy,
            total_cost,
            swap_cost,
            bond_breaking_cost,
            direction.x,
            direction.y,
            direction.y > 0 ? "DOWN"
                            : (direction.y < 0 ? "UP" : (direction.x > 0 ? "RIGHT" : "LEFT")));
    }
    else {

        LoggingChannels::swap()->info(
            "Swap approved: {} -> {} at ({},{}) -> ({},{}) | Energy: {:.3f} >= {:.3f} (base: "
            "{:.3f}, bonds: {:.3f}) | Dir: ({},{}) {}",
            getMaterialName(fromCell.material_type),
            getMaterialName(toCell.material_type),
            fromX,
            fromY,
            fromX + direction.x,
            fromY + direction.y,
            available_energy,
            total_cost,
            swap_cost,
            bond_breaking_cost,
            direction.x,
            direction.y,
            direction.y > 0 ? "DOWN"
                            : (direction.y < 0 ? "UP" : (direction.x > 0 ? "RIGHT" : "LEFT")));
    }

    return true;
}

void WorldCollisionCalculator::swapCounterMovingMaterials(
    Cell& fromCell, Cell& toCell, const Vector2i& direction, const MaterialMove& move)
{
    // Store material types before swap for logging.
    const MaterialType from_type = fromCell.material_type;
    const MaterialType to_type = toCell.material_type;

    // AIR swaps preserve momentum - no real collision occurred.
    // Moving through air should not cost energy (air resistance handled elsewhere).
    const bool involves_air = (from_type == MaterialType::AIR || to_type == MaterialType::AIR);

    Vector2d new_velocity;
    double swap_cost = 0.0;
    double remaining_energy = 0.0;

    if (involves_air) {
        // Preserve full momentum when swapping with air.
        new_velocity = move.momentum;
    }
    else {
        // Calculate swap cost for real material-material swaps.
        // Note: getEffectiveDensity() already includes fill_ratio, so don't multiply again.
        const double target_mass = toCell.getEffectiveDensity();
        swap_cost = 0.5 * target_mass * 1.0;

        // Calculate remaining energy after swap.
        // Energy is only lost proportional to work done (swap_cost).
        remaining_energy = std::max(0.0, move.collision_energy - swap_cost);

        // Get mass of moving material (fromCell -> toCell).
        const double moving_mass = fromCell.getEffectiveDensity();

        // Calculate new velocity magnitude for moving material after energy deduction.
        double velocity_magnitude_new = 0.0;
        if (moving_mass > 1e-6 && remaining_energy > 0.0) {
            velocity_magnitude_new = std::sqrt(2.0 * remaining_energy / moving_mass);
        }

        // Preserve velocity direction, but reduce magnitude.
        Vector2d velocity_direction =
            move.momentum.magnitude() > 1e-6 ? move.momentum.normalize() : Vector2d(0.0, 0.0);
        new_velocity = velocity_direction * velocity_magnitude_new;
    }

    // Swap material types and fill ratios (conserve mass).
    MaterialType temp_type = fromCell.material_type;
    double temp_fill = fromCell.fill_ratio;
    uint32_t temp_organism = fromCell.organism_id;

    fromCell.material_type = toCell.material_type;
    fromCell.fill_ratio = toCell.fill_ratio;
    fromCell.organism_id = toCell.organism_id;

    toCell.material_type = temp_type;
    toCell.fill_ratio = temp_fill;
    toCell.organism_id = temp_organism;

    // Moving material (now in toCell) continues trajectory with reduced velocity.
    // Calculate landing position based on boundary crossing trajectory.
    Vector2d landing_com =
        fromCell.calculateTrajectoryLanding(fromCell.com, move.momentum, move.boundary_normal);
    toCell.setCOM(landing_com);
    toCell.velocity = new_velocity;

    // Displaced material (now in fromCell) placed at center with zero velocity.
    fromCell.setCOM(Vector2d(0.0, 0.0));
    fromCell.velocity = Vector2d(0.0, 0.0);

    // Log with full details, INFO for non-air swaps, DEBUG for air swaps.
    const char* direction_str =
        direction.y > 0 ? "DOWN" : (direction.y < 0 ? "UP" : (direction.x > 0 ? "RIGHT" : "LEFT"));

    if (involves_air) {
        LoggingChannels::swap()->debug(
            "SWAP: {} <-> {} at ({},{}) <-> ({},{}) Dir:({},{}) {} | Vel: {:.3f} -> {:.3f} "
            "(air swap, momentum preserved) | landing_com: ({:.2f},{:.2f})",
            getMaterialName(from_type),
            getMaterialName(to_type),
            move.fromX,
            move.fromY,
            move.toX,
            move.toY,
            direction.x,
            direction.y,
            direction_str,
            move.momentum.magnitude(),
            new_velocity.magnitude(),
            landing_com.x,
            landing_com.y);
    }
    else {
        LoggingChannels::swap()->info(
            "SWAP: {} <-> {} at ({},{}) <-> ({},{}) Dir:({},{}) {} | Energy: {:.3f} - {:.3f} = "
            "{:.3f} | Vel: {:.3f} -> {:.3f} | landing_com: ({:.2f},{:.2f})",
            getMaterialName(from_type),
            getMaterialName(to_type),
            move.fromX,
            move.fromY,
            move.toX,
            move.toY,
            direction.x,
            direction.y,
            direction_str,
            move.collision_energy,
            swap_cost,
            remaining_energy,
            move.momentum.magnitude(),
            new_velocity.magnitude(),
            landing_com.x,
            landing_com.y);
    }
}

// =================================================================
// UTILITY METHODS.
// =================================================================

WorldCollisionCalculator::VelocityComponents WorldCollisionCalculator::decomposeVelocity(
    const Vector2d& velocity, const Vector2d& surface_normal) const
{
    VelocityComponents components;
    Vector2d normalized_normal = surface_normal.normalize();
    components.normal_scalar = velocity.dot(normalized_normal);
    components.normal = normalized_normal * components.normal_scalar;
    components.tangential = velocity - components.normal;
    return components;
}

bool WorldCollisionCalculator::isMaterialRigid(MaterialType material)
{
    switch (material) {
        case MaterialType::METAL:
        case MaterialType::WOOD:
        case MaterialType::WALL:
            return true;
        case MaterialType::WATER:
        case MaterialType::DIRT:
        case MaterialType::SAND:
        case MaterialType::LEAF:
        case MaterialType::AIR:
        default:
            return false;
    }
}

double WorldCollisionCalculator::calculateCohesionStrength(
    const Cell& cell, const World& world, uint32_t x, uint32_t y) const
{
    if (cell.isEmpty()) {
        return 0.0;
    }

    // Reuse existing cohesion calculation that includes support factor.
    WorldCohesionCalculator cohesion_calc;
    const auto cohesion_force = cohesion_calc.calculateCohesionForce(world, x, y);

    // Return the resistance magnitude (includes neighbors, fill ratio, and support factor).
    return cohesion_force.resistance_magnitude;
}
