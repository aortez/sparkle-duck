#include "WorldBPressureCalculator.h"
#include "CellB.h"
#include "WorldB.h"
#include "WorldInterface.h"

#include <algorithm>
#include <cmath>
#include <spdlog/spdlog.h>

WorldBPressureCalculator::WorldBPressureCalculator(WorldB& world)
    : WorldBCalculatorBase(world), world_ref_(world)
{}

void WorldBPressureCalculator::applyPressure(double deltaTime)
{
    if (world_ref_.getPressureScale() <= 0.0) {
        return;
    }

    // Apply pressure decay at start of timestep.
    for (uint32_t y = 0; y < world_ref_.getHeight(); ++y) {
        for (uint32_t x = 0; x < world_ref_.getWidth(); ++x) {
            CellB& cell = world_ref_.at(x, y);
            cell.setDynamicPressure(cell.getDynamicPressure() * 0.9);
        }
    }

    // Hydrostatic pressure will be added on top of any remaining pressure.
    if (world_ref_.isHydrostaticPressureEnabled()) {
        calculateHydrostaticPressure();
    }

    if (world_ref_.isDynamicPressureEnabled()) {
        // Generate virtual gravity transfers if hydrostatic is disabled.
        if (!world_ref_.isHydrostaticPressureEnabled()) {
            generateVirtualGravityTransfers(deltaTime);
        }
        
        processBlockedTransfers(blocked_transfers_);
        blocked_transfers_.clear();
        applyDynamicPressureForces(deltaTime);
    } else {
        blocked_transfers_.clear();
    }
}

void WorldBPressureCalculator::calculateHydrostaticPressure()
{
    // Skip if hydrostatic pressure is disabled.
    if (!world_ref_.isHydrostaticPressureEnabled()) {
        return;
    }

    // Slice-based hydrostatic pressure calculation, following under_pressure.md design.
    const Vector2d gravity = world_ref_.getGravityVector();
    const double gravity_magnitude = gravity.magnitude();

    if (gravity_magnitude < 0.0001) {
        return; // No gravity, no hydrostatic pressure.
    }

    // Process slices perpendicular to gravity direction.
    for (uint32_t x = 0; x < world_ref_.getWidth(); ++x) {
        double accumulated_pressure = 0.0;

        // Follow gravity direction from top to bottom.
        for (uint32_t y = 0; y < world_ref_.getHeight(); ++y) {
            CellB& cell = world_ref_.at(x, y);

            // Set hydrostatic pressure for this cell.
            cell.setHydrostaticPressure(accumulated_pressure);

            // Add this cell's contribution to pressure for cells below.
            // Use hydrostatic weight to properly exclude non-fluid materials like WALL.
            double effective_density = cell.getEffectiveDensity();
            if (effective_density > MIN_MATTER_THRESHOLD && !cell.isEmpty()) {
                double hydrostatic_weight = getHydrostaticWeight(cell.getMaterialType());
                accumulated_pressure += effective_density * hydrostatic_weight * gravity_magnitude * SLICE_THICKNESS;
            }
        }
    }
}

void WorldBPressureCalculator::queueBlockedTransfer(const BlockedTransfer& transfer)
{
    blocked_transfers_.push_back(transfer);
}

void WorldBPressureCalculator::processBlockedTransfers(const std::vector<BlockedTransfer>& blocked_transfers)
{
    // Process each blocked transfer.
    for (const auto& transfer : blocked_transfers) {
        // Determine where to apply pressure.
        bool apply_to_target = false;

        // Check if target cell is valid for pressure transmission.
        if (isValidCell(transfer.toX, transfer.toY)) {
            CellB& target_cell = world_ref_.at(transfer.toX, transfer.toY);

            // Check target cell type.
            if (target_cell.isWall()) {
                // Walls reflect pressure back to source.
                if (isValidCell(transfer.fromX, transfer.fromY)) {
                    CellB& source_cell = world_ref_.at(transfer.fromX, transfer.fromY);
                    
                    // Get material-specific dynamic weight for source.
                    double material_weight = getDynamicWeight(source_cell.getMaterialType());
                    double reflected_energy = transfer.energy * material_weight * 0.8; // 80% reflection coefficient.
                    
                    double current_pressure = source_cell.getDynamicPressure();
                    double new_pressure = current_pressure + reflected_energy;
                    
                    spdlog::debug(
                        "Blocked transfer from ({},{}) to WALL at ({},{}): amount={:.3f}, energy={:.3f}, "
                        "reflecting to SOURCE cell with material={}, weight={:.2f}, current_pressure={:.6f}, "
                        "new_pressure={:.6f}",
                        transfer.fromX,
                        transfer.fromY,
                        transfer.toX,
                        transfer.toY,
                        transfer.transfer_amount,
                        transfer.energy,
                        getMaterialName(source_cell.getMaterialType()),
                        material_weight,
                        current_pressure,
                        new_pressure);
                    
                    source_cell.setDynamicPressure(new_pressure);
                    
                    // Set debug dynamic pressure for visualization.
                    source_cell.setDebugDynamicPressure(source_cell.getDebugDynamicPressure() + reflected_energy);
                }
                continue;
            }
            else if (!target_cell.isEmpty()) {
                // Non-empty, non-wall target can receive pressure.
                apply_to_target = true;
            }
            else {
                // Empty cells - no pressure buildup.
                spdlog::debug(
                    "Blocked transfer from ({},{}) to ({},{}): target is empty - no pressure",
                    transfer.fromX,
                    transfer.fromY,
                    transfer.toX,
                    transfer.toY);
                continue;
            }
        }

        // Convert blocked kinetic energy to pressure.
        double blocked_energy = transfer.energy;

        if (apply_to_target) {
            // Apply pressure to target cell.
            CellB& target_cell = world_ref_.at(transfer.toX, transfer.toY);

            // Get material-specific dynamic weight.
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
            
            // Set debug dynamic pressure for visualization.
            target_cell.setDebugDynamicPressure(target_cell.getDebugDynamicPressure() + weighted_energy);
        }
    }
}

void WorldBPressureCalculator::applyDynamicPressureForces(double deltaTime)
{
    // Apply pressure forces and decay.
    for (uint32_t y = 0; y < world_ref_.getHeight(); ++y) {
        for (uint32_t x = 0; x < world_ref_.getWidth(); ++x) {
            CellB& cell = world_ref_.at(x, y);

            // Skip empty cells.
            if (cell.getFillRatio() < MIN_MATTER_THRESHOLD) {
                cell.setPressureGradient(Vector2d(0.0, 0.0));
                continue;
            }

            // Get total pressure for force calculation.
            double total_pressure = cell.getHydrostaticPressure() + cell.getDynamicPressure();
            if (total_pressure < MIN_PRESSURE_THRESHOLD) {
                cell.setPressureGradient(Vector2d(0.0, 0.0));
                continue;
            }

            // Calculate pressure gradient to determine force direction.
            Vector2d gradient = calculatePressureGradient(x, y);
            
            // Store gradient in cell for debug visualization.
            cell.setPressureGradient(gradient);
            
            // Force points DOWN the gradient (from high to low pressure)
            // For net gradient calculation in flow, we use gradient directly.
            // But for force application, we need to consider if this is equilibrium or not.
            Vector2d gravity_gradient = calculateGravityGradient(x, y);
            Vector2d net_gradient = gradient - gravity_gradient;
            
            // Apply force based on net gradient (deviation from equilibrium)
            Vector2d pressure_force = net_gradient * -1.0 * world_ref_.getPressureScale() 
                * DYNAMIC_MULTIPLIER * deltaTime;

            // Apply force to velocity.
            if (pressure_force.magnitude() > 0.0001) {
                Vector2d velocity_before = cell.getVelocity();
                Vector2d velocity_after = velocity_before + pressure_force;
                cell.setVelocity(velocity_after);

                spdlog::debug(
                    "Cell ({},{}) pressure force applied: pressure={:.4f}, "
                    "gradient=({:.4f},{:.4f}), force=({:.4f},{:.4f})",
                    x,
                    y,
                    total_pressure,
                    gradient.x,
                    gradient.y,
                    pressure_force.x,
                    pressure_force.y);
            }

            // Apply dynamic pressure decay (hydrostatic pressure doesn't decay)
            double dynamic_pressure = cell.getDynamicPressure();
            double new_dynamic_pressure = dynamic_pressure * (1.0 - DYNAMIC_DECAY_RATE * deltaTime);
            cell.setDynamicPressure(new_dynamic_pressure);
            
            // Decay debug dynamic pressure.
            double debug_dyn = cell.getDebugDynamicPressure();
            if (debug_dyn > 0.0001) {
                cell.setDebugDynamicPressure(debug_dyn * (1.0 - DYNAMIC_DECAY_RATE * deltaTime));
            }
        }
    }
}

// This method is no longer needed - pressure forces are calculated from gradients.
// in applyDynamicPressureForces.

double WorldBPressureCalculator::getHydrostaticWeight(MaterialType type) const
{
    // Material-specific hydrostatic pressure sensitivity.
    switch (type) {
        case MaterialType::WATER:
            return 1.0; // High hydrostatic sensitivity.
        case MaterialType::SAND:
        case MaterialType::DIRT:
            return 0.7; // Moderate hydrostatic sensitivity.
        case MaterialType::WOOD:
            return 0.3; // Low hydrostatic sensitivity.
        case MaterialType::METAL:
            return 0.1; // Very low hydrostatic sensitivity.
        case MaterialType::LEAF:
            return 0.4; // Low-moderate sensitivity.
        case MaterialType::WALL:
        case MaterialType::AIR:
        default:
            return 0.0; // No hydrostatic response.
    }
}

double WorldBPressureCalculator::getDynamicWeight(MaterialType type) const
{
    // Material-specific dynamic pressure sensitivity.
    switch (type) {
        case MaterialType::WATER:
            return 0.8; // High dynamic response.
        case MaterialType::SAND:
        case MaterialType::DIRT:
            return 1.0; // Full dynamic response.
        case MaterialType::WOOD:
        case MaterialType::METAL:
            return 0.5; // Moderate dynamic response (compression)
        case MaterialType::LEAF:
            return 0.6; // Moderate dynamic response.
        case MaterialType::WALL:
        case MaterialType::AIR:
        default:
            return 0.0; // No dynamic response.
    }
}

Vector2d WorldBPressureCalculator::calculatePressureGradient(uint32_t x, uint32_t y) const
{
    // Get center cell total pressure.
    const CellB& center = world_ref_.at(x, y);
    double center_pressure = center.getHydrostaticPressure() + center.getDynamicPressure();

    // Skip if no significant pressure.
    if (center_pressure < MIN_PRESSURE_THRESHOLD) {
        spdlog::trace(
            "Pressure gradient at ({},{}) - center pressure {:.6f} below threshold {:.6f}",
            x,
            y,
            center_pressure,
            MIN_PRESSURE_THRESHOLD);
        return Vector2d(0, 0);
    }

    Vector2d gradient(0, 0);
    int valid_neighbors = 0;

    // Check all 4 cardinal neighbors.
    const std::array<std::pair<int, int>, 4> directions = {
        { { -1, 0 }, { 1, 0 }, { 0, -1 }, { 0, 1 } }
    };

    for (const auto& [dx, dy] : directions) {
        int nx = static_cast<int>(x) + dx;
        int ny = static_cast<int>(y) + dy;

        if (isValidCell(nx, ny)) {
            const CellB& neighbor = world_ref_.at(nx, ny);

            // Skip walls - they block pressure gradient flow.
            if (neighbor.isWall()) {
                continue;
            }

            double neighbor_pressure = neighbor.getHydrostaticPressure() + neighbor.getDynamicPressure();

            // Gradient points from low to high pressure (like elevation gradient points uphill)
            // Flow goes DOWN the gradient (from high to low pressure)
            double pressure_diff = neighbor_pressure - center_pressure;

            // Accumulate gradient components.
            gradient.x += pressure_diff * dx;
            gradient.y += pressure_diff * dy;
            valid_neighbors++;

            spdlog::trace(
                "  Neighbor ({},{}) - pressure={:.6f}, diff={:.6f}, contribution=({:.6f},{:.6f})",
                nx,
                ny,
                neighbor_pressure,
                pressure_diff,
                pressure_diff * dx,
                pressure_diff * dy);
        }
    }

    // Average the gradient if we had valid neighbors.
    if (valid_neighbors > 0) {
        gradient = gradient / static_cast<double>(valid_neighbors);
    }

    spdlog::trace(
        "Pressure gradient at ({},{}) - center_pressure={:.6f}, gradient=({:.6f},{:.6f}), "
        "valid_neighbors={}",
        x,
        y,
        center_pressure,
        gradient.x,
        gradient.y,
        valid_neighbors);

    return gradient;
}

Vector2d WorldBPressureCalculator::calculateGravityGradient(uint32_t x, uint32_t y) const
{
    const CellB& center = world_ref_.at(x, y);
    double center_density = center.getEffectiveDensity();
    
    // Get gravity vector and magnitude.
    Vector2d gravity = world_ref_.getGravityVector();
    double gravity_magnitude = gravity.magnitude();
    
    // Skip if no gravity.
    if (gravity_magnitude < 0.001) {
        return Vector2d(0, 0);
    }
    
    Vector2d gravity_gradient(0, 0);
    int valid_neighbors = 0;
    
    // Check all 4 cardinal neighbors.
    const std::array<std::pair<int, int>, 4> directions = {
        { { -1, 0 }, { 1, 0 }, { 0, -1 }, { 0, 1 } }
    };
    
    for (const auto& [dx, dy] : directions) {
        int nx = static_cast<int>(x) + dx;
        int ny = static_cast<int>(y) + dy;
        
        if (isValidCell(nx, ny)) {
            const CellB& neighbor = world_ref_.at(nx, ny);
            
            // Skip walls - they don't contribute to gravity gradient.
            if (neighbor.isWall()) {
                continue;
            }
            
            // Calculate expected pressure difference due to gravity.
            // In the direction of gravity, pressure increases by density * gravity * distance.
            Vector2d direction(dx, dy);
            double gravity_component = gravity.dot(direction) * gravity_magnitude;
            
            // Expected pressure difference: neighbor should have higher pressure if it's "below" us.
            double expected_pressure_diff = center_density * gravity_component;
            
            // Accumulate gradient components.
            gravity_gradient.x += expected_pressure_diff * dx;
            gravity_gradient.y += expected_pressure_diff * dy;
            valid_neighbors++;
        }
    }
    
    // Average the gradient if we had valid neighbors.
    if (valid_neighbors > 0) {
        gravity_gradient = gravity_gradient / static_cast<double>(valid_neighbors);
    }
    
    return gravity_gradient;
}

std::vector<MaterialMove> WorldBPressureCalculator::calculatePressureFlow(double deltaTime)
{
    std::vector<MaterialMove> pressure_moves;

    // Iterate through all cells to find pressure-driven flows.
    for (uint32_t y = 0; y < world_ref_.getHeight(); y++) {
        for (uint32_t x = 0; x < world_ref_.getWidth(); x++) {
            CellB& cell = world_ref_.at(x, y);

            // Check if this cell has significant pressure that could drive flow.
            double total_pressure = cell.getHydrostaticPressure() + cell.getDynamicPressure();
            spdlog::trace(
                "Cell ({},{}) checking pressure flow: total_pressure={:.6f}, threshold={:.6f}",
                x,
                y,
                total_pressure,
                MIN_PRESSURE_THRESHOLD);

            if (total_pressure > MIN_PRESSURE_THRESHOLD
                && cell.getFillRatio() > MIN_MATTER_THRESHOLD
                && !cell.isWall()) {  // WALL materials should never flow.
                // Calculate pressure gradient.
                Vector2d pressure_gradient = calculatePressureGradient(x, y);
                
                // Calculate expected gravity gradient (equilibrium pressure gradient)
                Vector2d gravity_gradient = calculateGravityGradient(x, y);
                
                // Net gradient is pressure gradient minus gravity gradient.
                // This represents the actual disequilibrium driving flow.
                Vector2d net_gradient = pressure_gradient - gravity_gradient;

                spdlog::info(
                    "Cell ({},{}) pressure gradient: ({:.6f},{:.6f}), gravity gradient: ({:.6f},{:.6f}), net: ({:.6f},{:.6f})",
                    x,
                    y,
                    pressure_gradient.x,
                    pressure_gradient.y,
                    gravity_gradient.x,
                    gravity_gradient.y,
                    net_gradient.x,
                    net_gradient.y);

                // Only proceed if net gradient is significant (system out of equilibrium)
                if (net_gradient.magnitude() > 0.001) {
                    // Material flows DOWN the net gradient (from high to low pressure)
                    Vector2d flow_direction = net_gradient * -1.0;
                    flow_direction.normalize();

                    spdlog::info(
                        "Flow direction at ({},{}): ({:.3f},{:.3f})",
                        x,
                        y,
                        flow_direction.x,
                        flow_direction.y);

                    // Determine which neighboring cell to flow towards.
                    // Use dominant component to pick cardinal direction.
                    Vector2i target_direction(0, 0);
                    if (std::abs(flow_direction.x) >= std::abs(flow_direction.y)) {
                        target_direction.x = (flow_direction.x > 0) ? 1 : -1;
                    }
                    else {
                        target_direction.y = (flow_direction.y > 0) ? 1 : -1;
                    }

                    Vector2i targetPos = Vector2i(x, y) + target_direction;

                    spdlog::debug(
                        "Target direction: ({},{}), target pos: ({},{})",
                        target_direction.x,
                        target_direction.y,
                        targetPos.x,
                        targetPos.y);

                    // Check if target is valid and can receive material.
                    if (isValidCell(targetPos.x, targetPos.y)) {
                        CellB& targetCell = world_ref_.at(targetPos);

                        // Don't flow into walls or full cells.
                        if (!targetCell.isWall()
                            && targetCell.getCapacity() > MIN_MATTER_THRESHOLD) {
                            // Calculate flow amount based on net gradient magnitude.
                            // Higher net gradient = more flow.
                            double flow_amount = std::min(
                                cell.getFillRatio() * PRESSURE_FLOW_RATE * net_gradient.magnitude()
                                    * deltaTime,
                                std::min(cell.getFillRatio(), targetCell.getCapacity()));

                            if (flow_amount > MIN_MATTER_THRESHOLD) {
                                // Create pressure-driven move.
                                MaterialMove pressure_move;
                                pressure_move.fromX = x;
                                pressure_move.fromY = y;
                                pressure_move.toX = targetPos.x;
                                pressure_move.toY = targetPos.y;
                                pressure_move.amount = flow_amount;
                                pressure_move.material = cell.getMaterialType();
                                pressure_move.momentum = flow_direction
                                    * net_gradient.magnitude(); // Pressure-driven momentum.
                                pressure_move.boundary_normal =
                                    Vector2d(target_direction.x, target_direction.y);
                                pressure_move.collision_type = CollisionType::TRANSFER_ONLY;

                                pressure_moves.push_back(pressure_move);

                                spdlog::debug(
                                    "Pressure-driven flow: {} at ({},{}) -> ({},{}) - amount: "
                                    "{:.3f}, "
                                    "gradient: ({:.3f},{:.3f}), pressure: {:.3f}",
                                    getMaterialName(cell.getMaterialType()),
                                    x,
                                    y,
                                    targetPos.x,
                                    targetPos.y,
                                    flow_amount,
                                    net_gradient.x,
                                    net_gradient.y,
                                    total_pressure);
                            }
                        }
                    }
                }
            }
        }
    }

    if (!pressure_moves.empty()) {
        spdlog::debug("Calculated {} pressure-driven material transfers", pressure_moves.size());
    }

    return pressure_moves;
}

void WorldBPressureCalculator::applyPressureForces(double deltaTime)
{
    // This is now handled by applyDynamicPressureForces.
    // The unified pressure system applies forces based on gradients.
    applyDynamicPressureForces(deltaTime);
}

void WorldBPressureCalculator::generateVirtualGravityTransfers(double deltaTime)
{
    const Vector2d gravity = world_ref_.getGravityVector();
    const double gravity_magnitude = gravity.magnitude();
    
    if (gravity_magnitude < 0.0001) {
        return; // No gravity, no virtual transfers.
    }
    
    // Process all cells to generate virtual gravity transfers.
    for (uint32_t y = 0; y < world_ref_.getHeight(); ++y) {
        for (uint32_t x = 0; x < world_ref_.getWidth(); ++x) {
            CellB& cell = world_ref_.at(x, y);
            
            // Skip empty cells and walls.
            if (cell.getFillRatio() < MIN_MATTER_THRESHOLD || cell.isWall()) {
                continue;
            }
            
            // Calculate virtual downward velocity from gravity.
            Vector2d gravity_velocity = gravity * deltaTime;
            
            // Calculate virtual kinetic energy from gravitational acceleration.
            double velocity_squared = gravity_velocity.x * gravity_velocity.x + gravity_velocity.y * gravity_velocity.y;
            double virtual_energy = 0.5 * cell.getEffectiveDensity() * velocity_squared;
            
            // Check if downward motion would be blocked.
            // For now, assume gravity points down (0, 1).
            int below_x = x;
            int below_y = y + 1;
            
            bool would_be_blocked = false;
            if (isValidCell(below_x, below_y)) {
                const CellB& cell_below = world_ref_.at(below_x, below_y);
                // Consider blocked if cell below is nearly full or is a wall.
                if (cell_below.getFillRatio() > 0.8 || cell_below.isWall()) {
                    would_be_blocked = true;
                }
            } else {
                // At bottom boundary - always blocked.
                would_be_blocked = true;
            }
            
            if (would_be_blocked) {
                // Create a virtual blocked transfer.
                BlockedTransfer virtual_transfer;
                virtual_transfer.fromX = x;
                virtual_transfer.fromY = y;
                virtual_transfer.toX = below_x;
                virtual_transfer.toY = below_y;
                virtual_transfer.transfer_amount = cell.getFillRatio(); // Small virtual amount.
                virtual_transfer.velocity = gravity_velocity;
                virtual_transfer.energy = virtual_energy;
                
                // Queue this virtual transfer for pressure processing.
                queueBlockedTransfer(virtual_transfer);
                
                spdlog::trace(
                    "Virtual gravity transfer at ({},{}): energy={:.6f}, density={:.3f}",
                    x,
                    y,
                    virtual_energy,
                    cell.getEffectiveDensity());
            }
        }
    }
}
