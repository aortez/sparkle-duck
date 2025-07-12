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

    // Get hydrostatic pressure strength multiplier.
    const double hydrostatic_strength = world_ref_.getHydrostaticPressureStrength() * 0.1;

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
                accumulated_pressure += effective_density * hydrostatic_weight * gravity_magnitude
                    * SLICE_THICKNESS * hydrostatic_strength;
            }
        }
    }
}

void WorldBPressureCalculator::queueBlockedTransfer(const BlockedTransfer& transfer)
{
    blocked_transfers_.push_back(transfer);
}

void WorldBPressureCalculator::processBlockedTransfers(
    const std::vector<BlockedTransfer>& blocked_transfers)
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
                    double dynamic_strength = world_ref_.getDynamicPressureStrength();

                    // Calculate material-based reflection coefficient.
                    double reflection_coefficient = calculateReflectionCoefficient(
                        source_cell.getMaterialType(), transfer.energy);

                    double reflected_energy = transfer.energy * material_weight * dynamic_strength
                        * reflection_coefficient;

                    double current_pressure = source_cell.getDynamicPressure();
                    double new_pressure = current_pressure + reflected_energy;

                    spdlog::debug(
                        "Blocked transfer from ({},{}) to WALL at ({},{}): amount={:.3f}, "
                        "energy={:.3f}, "
                        "reflecting to SOURCE cell with material={}, weight={:.2f}, "
                        "reflection_coeff={:.2f}, "
                        "current_pressure={:.6f}, new_pressure={:.6f}",
                        transfer.fromX,
                        transfer.fromY,
                        transfer.toX,
                        transfer.toY,
                        transfer.transfer_amount,
                        transfer.energy,
                        getMaterialName(source_cell.getMaterialType()),
                        material_weight,
                        reflection_coefficient,
                        current_pressure,
                        new_pressure);

                    source_cell.setDynamicPressure(new_pressure);
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
            double dynamic_strength = world_ref_.getDynamicPressureStrength();
            double weighted_energy = blocked_energy * material_weight * dynamic_strength;

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
        }
    }
}

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
    double center_pressure = center.getPressure();

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

            double neighbor_pressure = neighbor.getPressure();

            double pressure_diff = center_pressure - neighbor_pressure;

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

            // Expected pressure difference: neighbor should have higher pressure if it's "below"
            // us.
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

void WorldBPressureCalculator::applyPressureDecay(double deltaTime)
{
    // Apply decay to unified pressure values.
    for (uint32_t y = 0; y < world_ref_.getHeight(); ++y) {
        for (uint32_t x = 0; x < world_ref_.getWidth(); ++x) {
            CellB& cell = world_ref_.at(x, y);

            // Apply pressure decay to the unified pressure.
            double pressure = cell.getPressure();
            if (pressure > MIN_PRESSURE_THRESHOLD) {
                double new_pressure = pressure * (1.0 - DYNAMIC_DECAY_RATE * deltaTime);
                cell.setPressure(new_pressure);
                
                // Update components proportionally for visualization.
                double decay_factor = new_pressure / pressure;
                cell.setComponents(
                    cell.getHydrostaticComponent() * decay_factor,
                    cell.getDynamicComponent() * decay_factor
                );
            }

            // Update pressure gradient for visualization.
            // This allows us to see pressure forces at the beginning of the next frame.
            if (cell.getFillRatio() >= MIN_MATTER_THRESHOLD && !cell.isWall()) {
                double total_pressure = cell.getPressure();
                if (total_pressure >= MIN_PRESSURE_THRESHOLD) {
                    Vector2d gradient = calculatePressureGradient(x, y);
                    cell.setPressureGradient(gradient);
                }
                else {
                    cell.setPressureGradient(Vector2d(0.0, 0.0));
                }
            }
            else {
                cell.setPressureGradient(Vector2d(0.0, 0.0));
            }
        }
    }
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
            double velocity_squared =
                gravity_velocity.x * gravity_velocity.x + gravity_velocity.y * gravity_velocity.y;
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
            }
            else {
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

double WorldBPressureCalculator::calculateReflectionCoefficient(
    MaterialType materialType, double impactEnergy) const
{
    // Get material elasticity from properties.
    const MaterialProperties& material_props = getMaterialProperties(materialType);
    double material_elasticity = material_props.elasticity;

    // Wall elasticity is fixed at 0.9 (from MaterialType.cpp).
    const double wall_elasticity = 0.9;

    // Calculate coefficient of restitution using geometric mean.
    // This models the interaction between two materials.
    double base_restitution = std::sqrt(material_elasticity * wall_elasticity);

    // Apply energy-dependent damping for more realistic behavior.
    // Higher energy impacts lose more energy due to deformation, heat, sound, etc.
    // Normalize energy to a reasonable scale (10.0 is a high-energy impact).
    double energy_damping_factor = 1.0 - (0.1 * std::min(1.0, impactEnergy / 10.0));

    // Final reflection coefficient combines material properties and energy damping.
    double reflection_coefficient = base_restitution * energy_damping_factor;

    spdlog::trace(
        "Reflection coefficient for {} hitting wall: elasticity={:.2f}, base_restitution={:.2f}, "
        "energy={:.3f}, energy_damping={:.2f}, final_coefficient={:.2f}",
        getMaterialName(materialType),
        material_elasticity,
        base_restitution,
        impactEnergy,
        energy_damping_factor,
        reflection_coefficient);

    return reflection_coefficient;
}

void WorldBPressureCalculator::applyPressureDiffusion(double deltaTime)
{
    // Create a temporary copy of pressure values to avoid order-dependent updates.
    const uint32_t width = world_ref_.getWidth();
    const uint32_t height = world_ref_.getHeight();
    std::vector<double> new_pressure(width * height);

    // Copy current pressure values.
    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            size_t idx = y * width + x;
            const CellB& cell = world_ref_.at(x, y);
            new_pressure[idx] = cell.getPressure();
        }
    }

    // Compile-time switch for 4 vs 8 neighbor diffusion.
    // 8-neighbor diffusion includes diagonal neighbors for smoother pressure propagation.
    // 4-neighbor diffusion is faster but may show more grid artifacts.
    constexpr bool USE_8_NEIGHBORS = true;  // Set to false for 4-neighbor diffusion.

    // Apply neighbor diffusion.
    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            size_t idx = y * width + x;
            const CellB& cell = world_ref_.at(x, y);

            // Skip empty cells and walls.
            if (cell.isEmpty() || cell.getMaterialType() == MaterialType::WALL) {
                continue;
            }

            // Get material diffusion coefficient.
            const MaterialProperties& props = getMaterialProperties(cell.getMaterialType());
            double diffusion_rate = props.pressure_diffusion;

            // Calculate pressure flux with neighbors.
            double pressure_flux = 0.0;
            const double current_pressure = cell.getPressure();

            if constexpr (USE_8_NEIGHBORS) {
                // Check all 8 neighbors (including diagonals).
                const int dx[] = { -1, 0, 1, -1, 1, -1, 0, 1 };
                const int dy[] = { -1, -1, -1, 0, 0, 1, 1, 1 };
                constexpr int num_neighbors = 8;

                for (int i = 0; i < num_neighbors; ++i) {
                    int nx = static_cast<int>(x) + dx[i];
                    int ny = static_cast<int>(y) + dy[i];

                    // Skip out-of-bounds.
                    if (nx < 0 || nx >= static_cast<int>(width) || ny < 0
                        || ny >= static_cast<int>(height)) {
                        continue;
                    }

                    const CellB& neighbor = world_ref_.at(nx, ny);

                    // Walls block pressure diffusion.
                    if (neighbor.getMaterialType() == MaterialType::WALL) {
                        continue;
                    }

                    // Empty cells act as pressure sinks.
                    double neighbor_pressure = neighbor.isEmpty() ? 0.0 : neighbor.getPressure();

                    // Pressure flows from high to low.
                    double pressure_diff = neighbor_pressure - current_pressure;

                    // Use harmonic mean of diffusion coefficients for interface.
                    double neighbor_diffusion = neighbor.isEmpty()
                        ? 1.0
                        : getMaterialProperties(neighbor.getMaterialType()).pressure_diffusion;

                    // Harmonic mean handles material boundaries correctly.
                    double interface_diffusion = 2.0 * diffusion_rate * neighbor_diffusion
                        / (diffusion_rate + neighbor_diffusion + 1e-10);

                    // For diagonal neighbors, scale by 1/sqrt(2) to account for distance.
                    if (dx[i] != 0 && dy[i] != 0) {
                        interface_diffusion *= 0.707107;  // 1/sqrt(2)
                    }

                    // Accumulate flux.
                    pressure_flux += interface_diffusion * pressure_diff;
                }
            } else {
                // Check 4 cardinal neighbors only (N, S, E, W).
                const int dx[] = { 0, 0, 1, -1 };
                const int dy[] = { -1, 1, 0, 0 };
                constexpr int num_neighbors = 4;

                for (int i = 0; i < num_neighbors; ++i) {
                    int nx = static_cast<int>(x) + dx[i];
                    int ny = static_cast<int>(y) + dy[i];

                    // Skip out-of-bounds.
                    if (nx < 0 || nx >= static_cast<int>(width) || ny < 0
                        || ny >= static_cast<int>(height)) {
                        continue;
                    }

                    const CellB& neighbor = world_ref_.at(nx, ny);

                    // Walls block pressure diffusion.
                    if (neighbor.getMaterialType() == MaterialType::WALL) {
                        continue;
                    }

                    // Empty cells act as pressure sinks.
                    double neighbor_pressure = neighbor.isEmpty() ? 0.0 : neighbor.getPressure();

                    // Pressure flows from high to low.
                    double pressure_diff = -neighbor_pressure + current_pressure;

                    // Use harmonic mean of diffusion coefficients for interface.
                    double neighbor_diffusion = neighbor.isEmpty()
                        ? 1.0
                        : getMaterialProperties(neighbor.getMaterialType()).pressure_diffusion;

                    // Harmonic mean handles material boundaries correctly.
                    double interface_diffusion = 2.0 * diffusion_rate * neighbor_diffusion
                        / (diffusion_rate + neighbor_diffusion + 1e-10);

                    // Accumulate flux.
                    pressure_flux += interface_diffusion * pressure_diff;
                }
            }

            // Update pressure with diffusion flux.
            // Scale by deltaTime for frame-rate independence.
            new_pressure[idx] = current_pressure + pressure_flux * deltaTime;

            // Ensure pressure doesn't go negative.
            if (new_pressure[idx] < 0.0) {
                new_pressure[idx] = 0.0;
            }
        }
    }

    // Apply the new pressure values.
    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            size_t idx = y * width + x;
            CellB& cell = world_ref_.at(x, y);
            double old_pressure = cell.getPressure();
            double new_unified_pressure = new_pressure[idx];
            cell.setPressure(new_unified_pressure);
            
            // Update components proportionally based on diffusion.
            if (old_pressure > 0.0) {
                double ratio = new_unified_pressure / old_pressure;
                cell.setComponents(
                    cell.getHydrostaticComponent() * ratio,
                    cell.getDynamicComponent() * ratio
                );
            }
        }
    }
}
