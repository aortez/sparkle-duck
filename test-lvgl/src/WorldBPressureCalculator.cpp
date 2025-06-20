#include "WorldBPressureCalculator.h"
#include "WorldB.h"
#include "CellB.h"
#include "WorldInterface.h" // For PressureSystem enum
#include <cmath>
#include <algorithm>

WorldBPressureCalculator::WorldBPressureCalculator(WorldB& world)
    : WorldBCalculatorBase(world), world_ref_(world)
{
}

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
        // Validate source cell
        if (!isValidCell(transfer.fromX, transfer.fromY)) {
            continue;
        }

        CellB& source_cell = world_ref_.at(transfer.fromX, transfer.fromY);
        
        // Convert blocked kinetic energy to dynamic pressure
        double blocked_energy = transfer.energy;
        double current_pressure = source_cell.getDynamicPressure();
        double new_pressure = current_pressure + blocked_energy * DYNAMIC_ACCUMULATION_RATE;
        
        // Cap maximum dynamic pressure
        new_pressure = std::min(new_pressure, 10.0); // Max from under_pressure.md
        source_cell.setDynamicPressure(new_pressure);

        // Update pressure gradient based on blocked direction
        Vector2d blocked_direction = transfer.velocity.normalize();
        Vector2d current_gradient = source_cell.getPressureGradient();
        Vector2d new_gradient = (current_gradient * current_pressure + 
                                blocked_direction * blocked_energy) / (current_pressure + blocked_energy);
        source_cell.setPressureGradient(new_gradient.normalize());
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

            // Calculate and apply pressure force
            Vector2d pressure_force = calculatePressureForce(cell);
            cell.setVelocity(cell.getVelocity() + pressure_force * deltaTime);

            // Decay dynamic pressure over time
            double dynamic_pressure = cell.getDynamicPressure();
            if (dynamic_pressure > MIN_PRESSURE_THRESHOLD) {
                cell.setDynamicPressure(dynamic_pressure * (1.0 - DYNAMIC_DECAY_RATE * deltaTime));
            } else {
                cell.setDynamicPressure(0.0);
            }
        }
    }
}

Vector2d WorldBPressureCalculator::calculatePressureForce(const CellB& cell) const
{
    // Combined pressure force calculation following design in under_pressure.md.
    
    // Hydrostatic component (gravity-aligned)
    Vector2d gravity_direction = world_ref_.getGravityVector().normalize();
    Vector2d hydrostatic_force = gravity_direction * cell.getHydrostaticPressure() * HYDROSTATIC_MULTIPLIER;
    
    // Dynamic component (blocked-transfer direction)
    Vector2d dynamic_force = cell.getPressureGradient() * cell.getDynamicPressure() * DYNAMIC_MULTIPLIER;
    
    // Material-specific weighting
    double hydrostatic_weight = getHydrostaticWeight(cell.getMaterialType());
    double dynamic_weight = getDynamicWeight(cell.getMaterialType());
    
    // Scale by global pressure setting
    double pressure_scale = world_ref_.getPressureScale();
    
    return (hydrostatic_force * hydrostatic_weight + dynamic_force * dynamic_weight) * pressure_scale;
}

double WorldBPressureCalculator::getHydrostaticWeight(MaterialType type) const
{
    // Material-specific hydrostatic pressure sensitivity
    switch (type) {
        case MaterialType::WATER:
            return 1.0;  // High hydrostatic sensitivity
        case MaterialType::SAND:
        case MaterialType::DIRT:
            return 0.7;  // Moderate hydrostatic sensitivity
        case MaterialType::WOOD:
            return 0.3;  // Low hydrostatic sensitivity
        case MaterialType::METAL:
            return 0.1;  // Very low hydrostatic sensitivity
        case MaterialType::LEAF:
            return 0.4;  // Low-moderate sensitivity
        case MaterialType::WALL:
        case MaterialType::AIR:
        default:
            return 0.0;  // No hydrostatic response
    }
}

double WorldBPressureCalculator::getDynamicWeight(MaterialType type) const
{
    // Material-specific dynamic pressure sensitivity
    switch (type) {
        case MaterialType::WATER:
            return 0.8;  // High dynamic response
        case MaterialType::SAND:
        case MaterialType::DIRT:
            return 1.0;  // Full dynamic response
        case MaterialType::WOOD:
        case MaterialType::METAL:
            return 0.5;  // Moderate dynamic response (compression)
        case MaterialType::LEAF:
            return 0.6;  // Moderate dynamic response
        case MaterialType::WALL:
        case MaterialType::AIR:
        default:
            return 0.0;  // No dynamic response
    }
}