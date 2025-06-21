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

Vector2d WorldBPressureCalculator::calculatePressureGradient(uint32_t x, uint32_t y) const
{
    // Get center cell pressure
    const CellB& center = world_ref_.at(x, y);
    double center_pressure = center.getHydrostaticPressure() + center.getDynamicPressure();

    // Skip if no significant pressure
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

    // Check all 4 cardinal neighbors
    const std::array<std::pair<int, int>, 4> directions = {
        { { -1, 0 }, { 1, 0 }, { 0, -1 }, { 0, 1 } }
    };

    for (const auto& [dx, dy] : directions) {
        int nx = static_cast<int>(x) + dx;
        int ny = static_cast<int>(y) + dy;

        if (isValidCell(nx, ny)) {
            const CellB& neighbor = world_ref_.at(nx, ny);

            // Skip walls - they block pressure gradient flow
            if (neighbor.isWall()) {
                continue;
            }

            double neighbor_pressure =
                neighbor.getHydrostaticPressure() + neighbor.getDynamicPressure();

            // Gradient points from low to high pressure (like elevation gradient points uphill)
            // Flow goes DOWN the gradient (from high to low pressure)
            double pressure_diff = neighbor_pressure - center_pressure;

            // Accumulate gradient components
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

    // Average the gradient if we had valid neighbors
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

std::vector<MaterialMove> WorldBPressureCalculator::calculatePressureFlow(double deltaTime)
{
    std::vector<MaterialMove> pressure_moves;

    // Iterate through all cells to find pressure-driven flows
    for (uint32_t y = 0; y < world_ref_.getHeight(); y++) {
        for (uint32_t x = 0; x < world_ref_.getWidth(); x++) {
            CellB& cell = world_ref_.at(x, y);

            // Check if this cell has significant pressure that could drive flow
            double total_pressure = cell.getHydrostaticPressure() + cell.getDynamicPressure();
            spdlog::trace(
                "Cell ({},{}) checking pressure flow: total_pressure={:.6f}, threshold={:.6f}",
                x,
                y,
                total_pressure,
                MIN_PRESSURE_THRESHOLD);

            if (total_pressure > MIN_PRESSURE_THRESHOLD
                && cell.getFillRatio() > MIN_MATTER_THRESHOLD) {
                // Calculate pressure gradient
                Vector2d gradient = calculatePressureGradient(x, y);

                spdlog::debug(
                    "Cell ({},{}) has pressure {:.6f}, gradient: ({:.6f},{:.6f}), magnitude: "
                    "{:.6f}",
                    x,
                    y,
                    total_pressure,
                    gradient.x,
                    gradient.y,
                    gradient.magnitude());

                // Only proceed if gradient is significant
                if (gradient.magnitude() > 0.001) {
                    // Material flows DOWN the gradient (from high to low pressure)
                    Vector2d flow_direction = gradient * -1.0;
                    flow_direction.normalize();

                    spdlog::debug(
                        "Flow direction at ({},{}): ({:.3f},{:.3f})",
                        x,
                        y,
                        flow_direction.x,
                        flow_direction.y);

                    // Determine which neighboring cell to flow towards
                    // Use dominant component to pick cardinal direction
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

                    // Check if target is valid and can receive material
                    if (isValidCell(targetPos.x, targetPos.y)) {
                        CellB& targetCell = world_ref_.at(targetPos);

                        // Don't flow into walls or full cells
                        if (!targetCell.isWall()
                            && targetCell.getCapacity() > MIN_MATTER_THRESHOLD) {
                            // Calculate flow amount based on pressure gradient magnitude
                            // Higher gradient = more flow
                            double flow_amount = std::min(
                                cell.getFillRatio() * PRESSURE_FLOW_RATE * gradient.magnitude()
                                    * deltaTime,
                                std::min(cell.getFillRatio(), targetCell.getCapacity()));

                            if (flow_amount > MIN_MATTER_THRESHOLD) {
                                // Create pressure-driven move
                                MaterialMove pressure_move;
                                pressure_move.fromX = x;
                                pressure_move.fromY = y;
                                pressure_move.toX = targetPos.x;
                                pressure_move.toY = targetPos.y;
                                pressure_move.amount = flow_amount;
                                pressure_move.material = cell.getMaterialType();
                                pressure_move.momentum = flow_direction
                                    * gradient.magnitude(); // Pressure-driven momentum
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
                                    gradient.x,
                                    gradient.y,
                                    total_pressure);
                            }
                        }
                    }
                }
            }
        }
    }

    if (!pressure_moves.empty()) {
        spdlog::info("Calculated {} pressure-driven material transfers", pressure_moves.size());
    }

    return pressure_moves;
}

void WorldBPressureCalculator::applyPressureForces(double deltaTime)
{
    // Apply pressure forces to velocities and handle pressure decay
    for (uint32_t y = 0; y < world_ref_.getHeight(); y++) {
        for (uint32_t x = 0; x < world_ref_.getWidth(); x++) {
            CellB& cell = world_ref_.at(x, y);
            double pressure = cell.getDynamicPressure();

            if (pressure > 0.001) {
                // Calculate pressure force
                Vector2d pressure_force =
                    cell.getPressureGradient() * pressure * PRESSURE_FORCE_SCALE * deltaTime;

                if (pressure_force.magnitude() > 0.001) {
                    // Cache pressure state for debug visualization before clearing
                    cell.setDebugPressure(pressure, cell.getPressureVector());

                    // Apply pressure force to velocity
                    cell.setVelocity(cell.getVelocity() + pressure_force);

                    // Dissipate the pressure that was converted to velocity
                    // Pressure is fully consumed when applied as force
                    cell.setDynamicPressure(0.0);

                    spdlog::trace(
                        "Cell ({},{}) pressure dissipated: {:.3f} -> 0.0, force applied: "
                        "({:.3f},{:.3f})",
                        x,
                        y,
                        pressure,
                        pressure_force.x,
                        pressure_force.y);
                }
                else {
                    // No significant force direction - just apply background decay
                    double new_pressure = pressure * (1.0 - BACKGROUND_DECAY_RATE * deltaTime);
                    cell.setDynamicPressure(new_pressure);

                    spdlog::trace(
                        "Cell ({},{}) pressure decay: {:.3f} -> {:.3f} (no force direction)",
                        x,
                        y,
                        pressure,
                        new_pressure);
                }
            }

            // Decay debug visualization cache
            if (cell.getDebugPressureMagnitude() > 0.001f) {
                cell.setDebugPressure(
                    cell.getDebugPressureMagnitude() * 0.9f, cell.getPressureVector());
                if (cell.getDebugPressureMagnitude() < 0.001f) {
                    cell.setDebugPressure(0.0f, Vector2d(0.0, 0.0));
                }
            }
        }
    }
}
