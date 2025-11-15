#include "WorldCollisionCalculator.h"
#include "Cell.h"
#include "MaterialMove.h"
#include "World.h"
#include "WorldCohesionCalculator.h"
#include "WorldPressureCalculator.h"
#include "spdlog/spdlog.h"
#include <algorithm>
#include <cmath>

using namespace DirtSim;

// =================================================================
// COLLISION DETECTION.
// =================================================================

std::vector<Vector2i> WorldCollisionCalculator::getAllBoundaryCrossings(
    const Vector2d& newCOM) const
{
    std::vector<Vector2i> crossings;

    // Check each boundary independently (aligned with original shouldTransfer logic)
    if (newCOM.x >= 1.0) crossings.push_back(Vector2i(1, 0));   // Right boundary.
    if (newCOM.x <= -1.0) crossings.push_back(Vector2i(-1, 0)); // Left boundary.
    if (newCOM.y >= 1.0) crossings.push_back(Vector2i(0, 1));   // Down boundary.
    if (newCOM.y <= -1.0) crossings.push_back(Vector2i(0, -1)); // Up boundary.

    return crossings;
}

MaterialMove WorldCollisionCalculator::createCollisionAwareMove(
    const World& world,
    const Cell& fromCell,
    const Cell& toCell,
    const Vector2i& fromPos,
    const Vector2i& toPos,
    const Vector2i& direction,
    double /* deltaTime. */,
    const WorldCohesionCalculator::COMCohesionForce& com_cohesion) const
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

    if (excess > MIN_MATTER_THRESHOLD && world.isDynamicPressureEnabled()) {
        double blocked_mass = excess * getMaterialDensity(fromCell.material_type);
        double energy = fromCell.velocity.magnitude() * blocked_mass;
        double dynamic_strength = world.getDynamicPressureStrength();
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

    // Add COM cohesion force data.
    move.com_cohesion_magnitude = com_cohesion.force_magnitude;
    move.com_cohesion_direction = com_cohesion.force_direction;

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
    double movingMass = calculateMaterialMass(fromCell) * move.amount;
    double velocity_magnitude = move.momentum.length();

    // If target cell has material, include reduced mass for collision.
    double targetMass = calculateMaterialMass(toCell);
    double effective_mass = movingMass;

    if (targetMass > 0.0) {
        // Reduced mass formula: μ = (m1 × m2) / (m1 + m2)
        effective_mass = (movingMass * targetMass) / (movingMass + targetMass);
    }

    return 0.5 * effective_mass * velocity_magnitude * velocity_magnitude;
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
        if (world.isDynamicPressureEnabled()) {
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

        spdlog::warn(
            "Elastic reflection: {} bounced off surface at ({},{}) with restitution {:.2f}, "
            "COM adjusted to ({:.3f},{:.3f})",
            getMaterialName(move.material),
            move.fromX,
            move.fromY,
            move.restitution_coefficient,
            fromCOM.x,
            fromCOM.y);
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
        world.isDynamicPressureEnabled(),
        transfer_deficit > MIN_MATTER_THRESHOLD,
        MIN_MATTER_THRESHOLD);

    if (transfer_deficit > MIN_MATTER_THRESHOLD && world.isDynamicPressureEnabled()) {
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

bool WorldCollisionCalculator::shouldSwapMaterials(
    const Cell& fromCell, const Cell& toCell, const Vector2i& direction) const
{
    // Only vertical swaps for now (buoyancy is primarily vertical).
    if (direction.y == 0) {
        return false;
    }

    // Dynamic pressure threshold - indicates gridlock from blocked transfers.
    // Lowered to 0.1 to enable gentle buoyancy-driven swapping.
    constexpr double MIN_DYNAMIC_PRESSURE = 0.1;

    // Check dynamic pressure (gridlock indicator).
    double from_dynamic = fromCell.dynamic_component;
    double to_dynamic = toCell.dynamic_component;
    bool pressure_indicates_gridlock =
        (from_dynamic > MIN_DYNAMIC_PRESSURE) || (to_dynamic > MIN_DYNAMIC_PRESSURE);

    spdlog::debug(
        "Swap check: from_dyn={:.2f} to_dyn={:.2f} -> {}",
        from_dynamic,
        to_dynamic,
        pressure_indicates_gridlock ? "SWAP" : "NO");

    return pressure_indicates_gridlock;
}

void WorldCollisionCalculator::swapCounterMovingMaterials(
    Cell& fromCell, Cell& toCell, const Vector2i& direction)
{
    spdlog::info(
        "SWAP: {} <-> {} (direction: {},{})",
        getMaterialName(fromCell.material_type),
        getMaterialName(toCell.material_type),
        direction.x,
        direction.y);

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

    // Zero velocities to prevent rapid re-swap.
    fromCell.velocity = Vector2d(0.0, 0.0);
    toCell.velocity = Vector2d(0.0, 0.0);

    // Center COMs to prevent immediate re-swap.
    fromCell.setCOM(Vector2d(0.0, 0.0));
    toCell.setCOM(Vector2d(0.0, 0.0));

    spdlog::info("SWAP complete: COMs repositioned for continued motion");
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
