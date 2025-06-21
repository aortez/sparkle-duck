#include "WorldBPressureCalculator.h"
#include "CellB.h"
#include "WorldB.h"
#include "WorldInterface.h" // For PressureSystem enum
#include <algorithm>
#include <cmath>
#include <spdlog/spdlog.h>

WorldBPressureCalculator::WorldBPressureCalculator(WorldB& world)
    : WorldBCalculatorBase(world), world_ref_(world)
{}

void WorldBPressureCalculator::applyPressure(double deltaTime)
{
    // Skip if pressure scale is zero (effectively disabled)
    if (world_ref_.getPressureScale() <= 0.0) {
        return;
    }

    // Original pressure system only supports hydrostatic for now
    if (world_ref_.getPressureSystem() == WorldInterface::PressureSystem::Original) {
        calculateHydrostaticPressure();
        return;
    }

    // Full dual pressure system
    if (world_ref_.isHydrostaticPressureEnabled()) {
        calculateHydrostaticPressure();
    }

    if (world_ref_.isDynamicPressureEnabled()) {
        processBlockedTransfers();
        applyDynamicPressureForces(deltaTime);
    }
}

void WorldBPressureCalculator::calculateHydrostaticPressure()
{
    // Skip if hydrostatic pressure is disabled
    if (!world_ref_.isHydrostaticPressureEnabled()) {
        return;
    }

    // Slice-based hydrostatic pressure calculation, following under_pressure.md design.
    const Vector2d gravity = world_ref_.getGravityVector();
    const double gravity_magnitude = gravity.magnitude();

    if (gravity_magnitude < 0.0001) {
        return; // No gravity, no hydrostatic pressure
    }

    // Process slices perpendicular to gravity direction
    for (uint32_t x = 0; x < world_ref_.getWidth(); ++x) {
        double accumulated_pressure = 0.0;

        // Follow gravity direction from top to bottom
        for (uint32_t y = 0; y < world_ref_.getHeight(); ++y) {
            CellB& cell = world_ref_.at(x, y);

            // Set current accumulated pressure on this cell.
            cell.setHydrostaticPressure(accumulated_pressure);

            // Add this cell's contribution to pressure for cells below.
            double effective_density = cell.getEffectiveDensity();
            if (effective_density > MIN_MATTER_THRESHOLD) {
                accumulated_pressure += effective_density * gravity_magnitude * SLICE_THICKNESS;
            }
        }
    }
}

void WorldBPressureCalculator::queueBlockedTransfer(const BlockedTransfer& transfer)
{
    blocked_transfers_.push_back(transfer);
}

void WorldBPressureCalculator::processBlockedTransfers()
{
    // Skip if dynamic pressure is disabled
    if (!world_ref_.isDynamicPressureEnabled()) {
        blocked_transfers_.clear();
        return;
    }

    // Process each blocked transfer
    for (const auto& transfer : blocked_transfers_) {
        // Determine where to apply pressure
        bool apply_to_target = false;

        // Check if target cell is valid for pressure transmission
        if (isValidCell(transfer.toX, transfer.toY)) {
            CellB& target_cell = world_ref_.at(transfer.toX, transfer.toY);

            // Check target cell type
            if (target_cell.isWall()) {
                // Walls eliminate pressure - skip this transfer
                spdlog::debug(
                    "Blocked transfer from ({},{}) to ({},{}): target is WALL - pressure "
                    "eliminated",
                    transfer.fromX,
                    transfer.fromY,
                    transfer.toX,
                    transfer.toY);
                continue;
            }
            else if (!target_cell.isEmpty()) {
                // Non-empty, non-wall target can receive pressure
                apply_to_target = true;
            }
            else {
                // Empty cells - no pressure buildup
                spdlog::debug(
                    "Blocked transfer from ({},{}) to ({},{}): target is empty - no pressure",
                    transfer.fromX,
                    transfer.fromY,
                    transfer.toX,
                    transfer.toY);
                continue;
            }
        }

        // Convert blocked kinetic energy to dynamic pressure
        double blocked_energy = transfer.energy;

        if (apply_to_target) {
            // Apply pressure to target cell
            CellB& target_cell = world_ref_.at(transfer.toX, transfer.toY);

            // Get material-specific dynamic weight
            double material_weight = getDynamicWeight(target_cell.getMaterialType());
            double weighted_energy = blocked_energy * material_weight;

            double current_pressure = target_cell.getDynamicPressure();
            double new_pressure = current_pressure + weighted_energy;

            spdlog::debug(
                "Blocked transfer from ({},{}) to ({},{}): amount={:.3f}, energy={:.3f}, "
                "applying to TARGET cell with material={}, weight={:.2f}, current_pressure={:.6f}, "
                "new_pressure={:.6f}",
                transfer.fromX,
                transfer.fromY,
                transfer.toX,
                transfer.toY,
                transfer.transfer_amount,
                blocked_energy,
                getMaterialName(target_cell.getMaterialType()),
                material_weight,
                current_pressure,
                new_pressure);

            target_cell.setDynamicPressure(new_pressure);

            // Update pressure vector - same direction as blocked velocity
            Vector2d blocked_direction = transfer.velocity;
            if (blocked_direction.magnitude() > 0.001) {
                blocked_direction.normalize();
                // Combine with existing pressure vector using weighted average
                Vector2d current_vector = target_cell.getPressureVector();
                Vector2d new_vector =
                    (current_vector * current_pressure + blocked_direction * weighted_energy)
                    / (current_pressure + weighted_energy);
                if (new_vector.magnitude() > 0.001) {
                    new_vector.normalize();
                    target_cell.setPressureVector(new_vector);
                }
            }
        }
    }

    // Clear processed transfers
    blocked_transfers_.clear();
}

void WorldBPressureCalculator::applyDynamicPressureForces(double deltaTime)
{
    // Apply pressure forces and decay
    for (uint32_t y = 0; y < world_ref_.getHeight(); ++y) {
        for (uint32_t x = 0; x < world_ref_.getWidth(); ++x) {
            CellB& cell = world_ref_.at(x, y);

            // Skip empty cells
            if (cell.getFillRatio() < MIN_MATTER_THRESHOLD) {
                continue;
            }

            // Get current dynamic pressure
            double dynamic_pressure = cell.getDynamicPressure();
            if (dynamic_pressure < MIN_PRESSURE_THRESHOLD) {
                continue;
            }

            // Calculate pressure force
            Vector2d pressure_force = calculatePressureForce(cell);

            // Apply FULL force to velocity without deltaTime scaling
            // This applies all pressure at once for clearer verification
            Vector2d velocity_before = cell.getVelocity();
            Vector2d velocity_after = velocity_before + pressure_force;
            cell.setVelocity(velocity_after);

            // Apply ALL pressure at once for verification
            // This helps verify if blocked force input is proportional to pressure force output
            // When pressure is applied, it should be fully consumed in one timestep

            // Extract just the dynamic component of the force
            Vector2d dynamic_force = cell.getPressureGradient() * dynamic_pressure
                * getDynamicWeight(cell.getMaterialType()) * world_ref_.getPressureScale()
                * DYNAMIC_MULTIPLIER;
            double dynamic_force_magnitude = dynamic_force.magnitude();

            if (dynamic_force_magnitude > 0.0001) {
                // Apply all dynamic pressure at once - full dissipation
                // This makes the relationship between blocked transfers and pressure forces clearer
                cell.setDynamicPressure(0.0);

                spdlog::debug(
                    "Cell ({},{}) full pressure dissipation: dynamic_force={:.4f}, "
                    "pressure {:.4f} -> 0.0 (fully applied)",
                    x,
                    y,
                    dynamic_force_magnitude,
                    dynamic_pressure);
            }
            else {
                // No significant force, just apply background decay
                cell.setDynamicPressure(dynamic_pressure * (1.0 - DYNAMIC_DECAY_RATE * deltaTime));
            }
        }
    }
}

Vector2d WorldBPressureCalculator::calculatePressureForce(const CellB& cell) const
{
    // Combined pressure force calculation following design in under_pressure.md.

    // Hydrostatic component (gravity-aligned)
    Vector2d gravity_direction = world_ref_.getGravityVector().normalize();
    Vector2d hydrostatic_force =
        gravity_direction * cell.getHydrostaticPressure() * HYDROSTATIC_MULTIPLIER;

    // Dynamic component (blocked-transfer direction)
    Vector2d dynamic_force =
        cell.getPressureGradient() * cell.getDynamicPressure() * DYNAMIC_MULTIPLIER;

    // Material-specific weighting
    double hydrostatic_weight = getHydrostaticWeight(cell.getMaterialType());
    double dynamic_weight = getDynamicWeight(cell.getMaterialType());

    // Scale by global pressure setting
    double pressure_scale = world_ref_.getPressureScale();

    return (hydrostatic_force * hydrostatic_weight + dynamic_force * dynamic_weight)
        * pressure_scale;
}

double WorldBPressureCalculator::getHydrostaticWeight(MaterialType type) const
{
    // Material-specific hydrostatic pressure sensitivity
    switch (type) {
        case MaterialType::WATER:
            return 1.0; // High hydrostatic sensitivity
        case MaterialType::SAND:
        case MaterialType::DIRT:
            return 0.7; // Moderate hydrostatic sensitivity
        case MaterialType::WOOD:
            return 0.3; // Low hydrostatic sensitivity
        case MaterialType::METAL:
            return 0.1; // Very low hydrostatic sensitivity
        case MaterialType::LEAF:
            return 0.4; // Low-moderate sensitivity
        case MaterialType::WALL:
        case MaterialType::AIR:
        default:
            return 0.0; // No hydrostatic response
    }
}

double WorldBPressureCalculator::getDynamicWeight(MaterialType type) const
{
    // Material-specific dynamic pressure sensitivity
    switch (type) {
        case MaterialType::WATER:
            return 0.8; // High dynamic response
        case MaterialType::SAND:
        case MaterialType::DIRT:
            return 1.0; // Full dynamic response
        case MaterialType::WOOD:
        case MaterialType::METAL:
            return 0.5; // Moderate dynamic response (compression)
        case MaterialType::LEAF:
            return 0.6; // Moderate dynamic response
        case MaterialType::WALL:
        case MaterialType::AIR:
        default:
            return 0.0; // No dynamic response
    }
}
