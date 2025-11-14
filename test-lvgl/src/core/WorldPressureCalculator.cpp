#include "WorldPressureCalculator.h"
#include "Cell.h"
#include "World.h"

#include <algorithm>
#include <cmath>
#include <spdlog/spdlog.h>

using namespace DirtSim;

void WorldPressureCalculator::calculateHydrostaticPressure(World& world)
{
    // Skip if hydrostatic pressure is disabled.
    if (!world.isHydrostaticPressureEnabled()) {
        return;
    }

    const Vector2d gravity = world.getGravityVector();
    const double gravity_magnitude = gravity.magnitude();

    if (gravity_magnitude < 0.0001) {
        return; // No gravity, no hydrostatic pressure.
    }

    const double hydrostatic_strength =
        world.getHydrostaticPressureStrength() * HYDROSTATIC_MULTIPLIER;

    // Compile-time switch for buoyancy calculation method.
    // COLUMN_BASED (true): Fast, column-independent tracking of fluid environment.
    // NEIGHBOR_BASED (false): More accurate, checks all neighbors for fluid density.
    constexpr bool USE_COLUMN_BASED_BUOYANCY = true;

    // Process each column independently.
    for (uint32_t x = 0; x < world.data.width; ++x) {
        // Top-down pressure accumulation with buoyancy support.
        double accumulated_pressure = 0.0;
        double current_fluid_density = 0.001; // Start assuming air environment.

        for (uint32_t y = 0; y < world.data.height; ++y) {
            Cell& cell = world.at(x, y);

            if (cell.isEmpty()) {
                // Empty cells: reset accumulation, no pressure.
                accumulated_pressure = 0.0;
                current_fluid_density = 0.001; // Reset to air.
                continue;
            }

            // All non-empty cells receive accumulated pressure.
            // This enables buoyancy for floating objects (no support required).
            double current_pressure = cell.pressure;
            cell.pressure = current_pressure + accumulated_pressure;
            cell.setHydrostaticPressure(accumulated_pressure);

            // Determine density contribution for accumulation (buoyancy calculation).
            double contributing_density;
            const MaterialProperties& props = getMaterialProperties(cell.material_type);

            if (props.is_fluid) {
                // Fluids contribute their own density.
                contributing_density = cell.getEffectiveDensity();
                current_fluid_density = contributing_density; // Update fluid environment tracker.
            }
            else {
                // Solids contribute surrounding fluid density (for proper buoyancy).
                if constexpr (USE_COLUMN_BASED_BUOYANCY) {
                    // Option C: Use current fluid environment (fast, column-independent).
                    contributing_density = current_fluid_density;
                }
                else {
                    // Option A: Query all neighbors for fluid density (more accurate).
                    contributing_density = getSurroundingFluidDensity(world, x, y);
                }
            }

            // Accumulate pressure for next cell below.
            accumulated_pressure +=
                contributing_density * gravity_magnitude * SLICE_THICKNESS * hydrostatic_strength;
        }
    }
}

void WorldPressureCalculator::queueBlockedTransfer(const BlockedTransfer& transfer)
{
    blocked_transfers_.push_back(transfer);
}

void WorldPressureCalculator::processBlockedTransfers(
    World& world, const std::vector<BlockedTransfer>& blocked_transfers)
{
    // Process each blocked transfer.
    for (const auto& transfer : blocked_transfers) {
        // Determine where to apply pressure.
        bool apply_to_target = false;

        // Check if target cell is valid for pressure transmission.
        if (isValidCell(world, transfer.toX, transfer.toY)) {
            Cell& target_cell = world.at(transfer.toX, transfer.toY);

            // Check target cell type.
            if (target_cell.isWall()) {
                // Walls reflect pressure back to source.
                if (isValidCell(world, transfer.fromX, transfer.fromY)) {
                    Cell& source_cell = world.at(transfer.fromX, transfer.fromY);

                    // Get material-specific dynamic weight for source.
                    double material_weight = getDynamicWeight(source_cell.material_type);
                    double dynamic_strength = world.getDynamicPressureStrength();

                    // Calculate material-based reflection coefficient.
                    double reflection_coefficient =
                        calculateReflectionCoefficient(source_cell.material_type, transfer.energy);

                    double reflected_energy = transfer.energy * material_weight * dynamic_strength
                        * reflection_coefficient;

                    // Add to unified pressure for persistence.
                    double current_unified_pressure = source_cell.pressure;
                    double new_unified_pressure = current_unified_pressure + reflected_energy;
                    source_cell.pressure = new_unified_pressure;

                    // Update dynamic component for visualization.
                    double current_dynamic = source_cell.dynamic_component;
                    source_cell.setDynamicPressure(current_dynamic + reflected_energy);

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
                        getMaterialName(source_cell.material_type),
                        material_weight,
                        reflection_coefficient,
                        current_unified_pressure,
                        new_unified_pressure);
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
            Cell& target_cell = world.at(transfer.toX, transfer.toY);

            // Get material-specific dynamic weight.
            double material_weight = getDynamicWeight(target_cell.material_type);
            double dynamic_strength = world.getDynamicPressureStrength();
            double weighted_energy = blocked_energy * material_weight * dynamic_strength;

            // Add to unified pressure for persistence.
            double current_unified_pressure = target_cell.pressure;
            double new_unified_pressure = current_unified_pressure + weighted_energy;
            target_cell.pressure = new_unified_pressure;

            // Update dynamic component for visualization.
            double current_dynamic = target_cell.dynamic_component;
            target_cell.setDynamicPressure(current_dynamic + weighted_energy);

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
                getMaterialName(target_cell.material_type),
                material_weight,
                current_unified_pressure,
                new_unified_pressure);
        }
    }
}

double WorldPressureCalculator::getHydrostaticWeight(MaterialType type) const
{
    return getMaterialProperties(type).hydrostatic_weight;
}

double WorldPressureCalculator::getDynamicWeight(MaterialType type) const
{
    return getMaterialProperties(type).dynamic_weight;
}

Vector2d WorldPressureCalculator::calculatePressureGradient(
    const World& world, uint32_t x, uint32_t y) const
{
    // Component-wise central difference gradient calculation.
    // This is the standard CFD approach: calculate each dimension independently
    // using only the aligned neighbors (not diagonal mixing).
    //
    // ∂P/∂x ≈ (P_right - P_left) / 2Δx
    // ∂P/∂y ≈ (P_down - P_up) / 2Δy

    const Cell& center = world.at(x, y);
    double center_pressure = center.pressure;

    // Skip if no significant pressure.
    if (center_pressure < MIN_PRESSURE_THRESHOLD) {
        spdlog::trace(
            "Pressure gradient at ({},{}) - center pressure {:.6f} below threshold {:.6f}",
            x,
            y,
            center_pressure,
            MIN_PRESSURE_THRESHOLD);
        return Vector2d{ 0, 0 };
    }

    Vector2d gradient(0, 0);

    // Calculate HORIZONTAL gradient (∂P/∂x) using left and right neighbors.
    {
        double p_left = center_pressure; // Default to center if no neighbor.
        double p_right = center_pressure;
        bool has_left = false;
        bool has_right = false;

        // Left neighbor.
        if (x > 0) {
            const Cell& left = world.at(x - 1, y);
            if (!left.isWall()) {
                p_left = left.pressure; // Use actual pressure (0 for empty cells).
                has_left = true;
            }
        }

        // Right neighbor.
        if (x < world.data.width - 1) {
            const Cell& right = world.at(x + 1, y);
            if (!right.isWall()) {
                p_right = right.pressure; // Use actual pressure (0 for empty cells).
                has_right = true;
            }
        }

        // Central difference: gradient points from high to low pressure (negate derivative).
        // Convention: negative gradient = pressure increasing in positive direction.
        if (has_left && has_right) {
            gradient.x = -(p_right - p_left) / 2.0;
        }
        else if (has_left) {
            gradient.x = -(center_pressure - p_left);
        }
        else if (has_right) {
            gradient.x = -(p_right - center_pressure);
        }
        // else: no horizontal neighbors, gradient.x = 0
    }

    // Calculate VERTICAL gradient (∂P/∂y) using up and down neighbors.
    {
        double p_up = center_pressure;
        double p_down = center_pressure;
        bool has_up = false;
        bool has_down = false;

        // Up neighbor.
        if (y > 0) {
            const Cell& up = world.at(x, y - 1);
            if (!up.isWall() && !up.isEmpty()) {
                p_up = up.pressure;
                has_up = true;
            }
        }

        // Down neighbor.
        if (y < world.data.height - 1) {
            const Cell& down = world.at(x, y + 1);
            if (!down.isWall() && !down.isEmpty()) {
                p_down = down.pressure;
                has_down = true;
            }
        }

        // Central difference: gradient points from high to low pressure (negate derivative).
        // Convention: negative gradient = pressure increasing in positive direction.
        if (has_up && has_down) {
            gradient.y = -(p_down - p_up) / 2.0;
        }
        else if (has_up) {
            gradient.y = -(center_pressure - p_up);
        }
        else if (has_down) {
            gradient.y = -(p_down - center_pressure);
        }
        // else: no vertical neighbors, gradient.y = 0
    }

    spdlog::trace(
        "Pressure gradient at ({},{}) - center={:.4f}, gradient=({:.4f},{:.4f})",
        x,
        y,
        center_pressure,
        gradient.x,
        gradient.y);

    return gradient;
}

Vector2d WorldPressureCalculator::calculateGravityGradient(
    const World& world, uint32_t x, uint32_t y) const
{
    const Cell& center = world.at(x, y);
    double center_density = center.getEffectiveDensity();

    // Get gravity vector and magnitude.
    Vector2d gravity = world.getGravityVector();
    double gravity_magnitude = gravity.magnitude();

    // Skip if no gravity.
    if (gravity_magnitude < 0.001) {
        return Vector2d{ 0, 0 };
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

        if (isValidCell(world, nx, ny)) {
            const Cell& neighbor = world.at(nx, ny);

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

void WorldPressureCalculator::applyPressureDecay(World& world, double deltaTime)
{
    // Apply decay to dynamic pressure only (not hydrostatic).
    // Hydrostatic pressure is recalculated each frame based on material positions,
    // so it doesn't need decay. Only dynamic pressure from collisions should dissipate.
    for (uint32_t y = 0; y < world.data.height; ++y) {
        for (uint32_t x = 0; x < world.data.width; ++x) {
            Cell& cell = world.at(x, y);

            // Only decay the dynamic component.
            double dynamic = cell.dynamic_component;
            if (dynamic > MIN_PRESSURE_THRESHOLD) {
                double new_dynamic = dynamic * (1.0 - DYNAMIC_DECAY_RATE * deltaTime);
                cell.setDynamicPressure(new_dynamic);

                // Recalculate total pressure as hydrostatic + decayed dynamic.
                double total = cell.hydrostatic_component + new_dynamic;
                cell.pressure = total;
            }

            // Update pressure gradient for visualization.
            // This allows us to see pressure forces at the beginning of the next frame.
            if (cell.fill_ratio >= MIN_MATTER_THRESHOLD && !cell.isWall()) {
                double total_pressure = cell.pressure;
                if (total_pressure >= MIN_PRESSURE_THRESHOLD) {
                    Vector2d gradient = calculatePressureGradient(world, x, y);
                    cell.pressure_gradient = gradient;
                }
                else {
                    cell.pressure_gradient = Vector2d{ 0.0, 0.0 };
                }
            }
            else {
                cell.pressure_gradient = Vector2d{ 0.0, 0.0 };
            }
        }
    }
}

void WorldPressureCalculator::generateVirtualGravityTransfers(World& world, double deltaTime)
{
    const Vector2d gravity = world.getGravityVector();
    const double gravity_magnitude = gravity.magnitude();

    if (gravity_magnitude < 0.0001) {
        return; // No gravity, no virtual transfers.
    }

    // Process all cells to generate virtual gravity transfers.
    for (uint32_t y = 0; y < world.data.height; ++y) {
        for (uint32_t x = 0; x < world.data.width; ++x) {
            Cell& cell = world.at(x, y);

            // Skip empty cells and walls.
            if (cell.fill_ratio < MIN_MATTER_THRESHOLD || cell.isWall()) {
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
            if (isValidCell(world, below_x, below_y)) {
                const Cell& cell_below = world.at(below_x, below_y);
                // Consider blocked if cell below is nearly full or is a wall.
                if (cell_below.fill_ratio > 0.8 || cell_below.isWall()) {
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
                virtual_transfer.transfer_amount = cell.fill_ratio;
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

double WorldPressureCalculator::calculateReflectionCoefficient(
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

bool WorldPressureCalculator::isRigidSupport(MaterialType type) const
{
    // Materials that can support weight above them.
    switch (type) {
        case MaterialType::WALL:
        case MaterialType::METAL:
        case MaterialType::WOOD:
            return true;
        case MaterialType::DIRT:
        case MaterialType::SAND:
            // Could add density check here for packed dirt/sand.
            return false; // For now, treat as non-rigid.
        default:
            return false;
    }
}

double WorldPressureCalculator::getSurroundingFluidDensity(
    const World& world, uint32_t x, uint32_t y) const
{
    // Calculate average fluid density from all 8 neighbors.
    // Used for accurate buoyancy calculation when USE_COLUMN_BASED_BUOYANCY = false.

    double total_fluid_density = 0.0;
    int fluid_neighbor_count = 0;

    // Check all 8 neighbors.
    const int dx[] = { -1, 0, 1, -1, 1, -1, 0, 1 };
    const int dy[] = { -1, -1, -1, 0, 0, 1, 1, 1 };

    for (int i = 0; i < 8; ++i) {
        int nx = static_cast<int>(x) + dx[i];
        int ny = static_cast<int>(y) + dy[i];

        // Check bounds.
        if (nx < 0 || nx >= static_cast<int>(world.data.width) || ny < 0
            || ny >= static_cast<int>(world.data.height)) {
            continue; // Out of bounds.
        }

        const Cell& neighbor = world.at(nx, ny);

        // Only count fluid neighbors (WATER, AIR).
        if (!neighbor.isEmpty()) {
            const MaterialProperties& neighbor_props =
                getMaterialProperties(neighbor.material_type);
            if (neighbor_props.is_fluid) {
                total_fluid_density += neighbor.getEffectiveDensity();
                fluid_neighbor_count++;
            }
        }
    }

    // Return average fluid density, or default to water density if no fluid neighbors.
    if (fluid_neighbor_count > 0) {
        return total_fluid_density / fluid_neighbor_count;
    }
    else {
        // No fluid neighbors found - default to water density (1.0).
        // This handles edge case of solid objects with no adjacent fluids.
        return 1.0;
    }
}

void WorldPressureCalculator::applyPressureDiffusion(World& world, double deltaTime)
{
    // Create a temporary copy of pressure values to avoid order-dependent updates.
    const uint32_t width = world.data.width;
    const uint32_t height = world.data.height;
    std::vector<double> new_pressure(width * height);

    // Copy current pressure values.
    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            size_t idx = y * width + x;
            const Cell& cell = world.at(x, y);
            new_pressure[idx] = cell.pressure;
        }
    }

    // Compile-time switch for 4 vs 8 neighbor diffusion.
    // 8-neighbor diffusion includes diagonal neighbors for smoother pressure propagation.
    // 4-neighbor diffusion is faster but may show more grid artifacts.
    constexpr bool USE_8_NEIGHBORS = true; // Set to false for 4-neighbor diffusion.

    // Apply neighbor diffusion.
    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            size_t idx = y * width + x;
            const Cell& cell = world.at(x, y);

            // Skip empty cells and walls.
            if (cell.isEmpty() || cell.material_type == MaterialType::WALL) {
                continue;
            }

            // Get material diffusion coefficient.
            const MaterialProperties& props = getMaterialProperties(cell.material_type);
            double diffusion_rate =
                props.pressure_diffusion * world.physicsSettings.pressure_diffusion_strength;

            // Calculate pressure flux with neighbors.
            double pressure_flux = 0.0;
            const double current_pressure = cell.pressure;

            if constexpr (USE_8_NEIGHBORS) {
                // Check all 8 neighbors (including diagonals).
                const int dx[] = { -1, 0, 1, -1, 1, -1, 0, 1 };
                const int dy[] = { -1, -1, -1, 0, 0, 1, 1, 1 };
                constexpr int num_neighbors = 8;

                for (int i = 0; i < num_neighbors; ++i) {
                    int nx = static_cast<int>(x) + dx[i];
                    int ny = static_cast<int>(y) + dy[i];

                    double neighbor_pressure;
                    double neighbor_diffusion;

                    // Handle out-of-bounds with ghost cell method.
                    if (nx < 0 || nx >= static_cast<int>(width) || ny < 0
                        || ny >= static_cast<int>(height)) {
                        // Ghost cell: same pressure as current cell (no-flux boundary).
                        neighbor_pressure = current_pressure;
                        neighbor_diffusion = diffusion_rate; // Same material properties.
                    }
                    else {
                        const Cell& neighbor = world.at(nx, ny);

                        // Walls act as no-flux boundaries (ghost cell method).
                        if (neighbor.material_type == MaterialType::WALL) {
                            neighbor_pressure = current_pressure;
                            neighbor_diffusion = diffusion_rate;
                        }
                        // Empty cells act as pressure sinks.
                        else if (neighbor.isEmpty()) {
                            neighbor_pressure = 0.0;
                            neighbor_diffusion = 1.0;
                        }
                        // Normal material cells.
                        else {
                            neighbor_pressure = neighbor.pressure;
                            neighbor_diffusion =
                                getMaterialProperties(neighbor.material_type).pressure_diffusion;
                        }
                    }

                    // Pressure flows from high to low.
                    double pressure_diff = neighbor_pressure - current_pressure;

                    // Harmonic mean handles material boundaries correctly.
                    double interface_diffusion = 2.0 * diffusion_rate * neighbor_diffusion
                        / (diffusion_rate + neighbor_diffusion + 1e-10);

                    // For diagonal neighbors, scale by 1/sqrt(2) to account for distance.
                    if (dx[i] != 0 && dy[i] != 0) {
                        interface_diffusion *= 0.707107; // 1/sqrt(2)
                    }

                    // Accumulate flux.
                    pressure_flux += interface_diffusion * pressure_diff;
                }
            }
            else {
                // Check 4 cardinal neighbors only (N, S, E, W).
                const int dx[] = { 0, 0, 1, -1 };
                const int dy[] = { -1, 1, 0, 0 };
                constexpr int num_neighbors = 4;

                for (int i = 0; i < num_neighbors; ++i) {
                    int nx = static_cast<int>(x) + dx[i];
                    int ny = static_cast<int>(y) + dy[i];

                    double neighbor_pressure;
                    double neighbor_diffusion;

                    // Handle out-of-bounds with ghost cell method.
                    if (nx < 0 || nx >= static_cast<int>(width) || ny < 0
                        || ny >= static_cast<int>(height)) {
                        // Ghost cell: same pressure as current cell (no-flux boundary).
                        neighbor_pressure = current_pressure;
                        neighbor_diffusion = diffusion_rate; // Same material properties.
                    }
                    else {
                        const Cell& neighbor = world.at(nx, ny);

                        // Walls act as no-flux boundaries (ghost cell method).
                        if (neighbor.material_type == MaterialType::WALL) {
                            neighbor_pressure = current_pressure;
                            neighbor_diffusion = diffusion_rate;
                        }
                        // Empty cells act as pressure sinks.
                        else if (neighbor.isEmpty()) {
                            neighbor_pressure = 0.0;
                            neighbor_diffusion = 1.0;
                        }
                        // Normal material cells.
                        else {
                            neighbor_pressure = neighbor.pressure;
                            neighbor_diffusion =
                                getMaterialProperties(neighbor.material_type).pressure_diffusion;
                        }
                    }

                    double pressure_diff = neighbor_pressure - current_pressure;

                    // Harmonic mean handles material boundaries correctly.
                    double interface_diffusion = 2.0 * diffusion_rate * neighbor_diffusion
                        / (diffusion_rate + neighbor_diffusion + 1e-10);

                    // Accumulate flux.
                    pressure_flux += interface_diffusion * pressure_diff;
                }
            }

            // Update pressure with diffusion flux.
            // Scale by deltaTime for frame-rate independence.
            // Apply stability limiter to prevent numerical instability.
            double pressure_change = pressure_flux * deltaTime;

            // Limit maximum pressure change per timestep to prevent explosions.
            // This ensures CFL stability for explicit diffusion scheme.
            // Symmetric 50% limit prevents both runaway growth and oscillations.
            double max_change = current_pressure * 0.5;
            if (std::abs(pressure_change) > max_change) {
                pressure_change = std::copysign(max_change, pressure_change);
            }

            new_pressure[idx] = current_pressure + pressure_change;

            // Ensure pressure doesn't go negative.
            if (new_pressure[idx] < 0.0) {
                new_pressure[idx] = 0.0;
            }
        }
    }

    // // Apply wall reflections before finalizing pressure values.
    // for (uint32_t y = 0; y < height; ++y) {
    //     for (uint32_t x = 0; x < width; ++x) {
    //         // Skip cells with no blocked flux.
    //         if (wall_reflections_[y][x].blocked_flux <= 0.0) {
    //             continue;
    //         }
    //
    //         size_t idx = y * width + x;
    //         const Cell& cell = world.at(x, y);
    //
    //         // Calculate reflection coefficient based on material.
    //         double reflection_coeff = calculateDiffusionReflectionCoefficient(
    //             cell.material_type, wall_reflections_[y][x].blocked_flux);
    //
    //         // Apply reflected pressure.
    //         double reflected_pressure = wall_reflections_[y][x].blocked_flux * reflection_coeff;
    //         new_pressure[idx] += reflected_pressure * deltaTime;
    //
    //         // Ensure pressure stays positive.
    //         if (new_pressure[idx] < 0.0) {
    //             new_pressure[idx] = 0.0;
    //         }
    //
    //         spdlog::trace(
    //             "Pressure reflection at ({},{}) - material={}, blocked_flux={:.6f}, "
    //             "reflection_coeff={:.3f}, added_pressure={:.6f}",
    //             x,
    //             y,
    //             getMaterialName(cell.material_type),
    //             wall_reflections_[y][x].blocked_flux,
    //             reflection_coeff,
    //             reflected_pressure * deltaTime);
    //     }
    // }

    // Apply the new pressure values.
    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            size_t idx = y * width + x;
            Cell& cell = world.at(x, y);
            double old_pressure = cell.pressure;
            double new_unified_pressure = new_pressure[idx];

            // Diffusion only affects dynamic component (transient waves).
            // Hydrostatic pressure is recalculated from geometry each frame.
            double pressure_change = new_unified_pressure - old_pressure;
            double new_dynamic = cell.dynamic_component + pressure_change;
            new_dynamic = std::max(0.0, new_dynamic); // Can't go negative.

            cell.dynamic_component = new_dynamic;
            cell.pressure = cell.hydrostatic_component + new_dynamic;
        }
    }
}
