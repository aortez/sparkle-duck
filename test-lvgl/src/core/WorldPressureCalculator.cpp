#include "WorldPressureCalculator.h"
#include "Cell.h"
#include "PhysicsSettings.h"
#include "World.h"
#include "WorldData.h"

#include <algorithm>
#include <cmath>
#include <spdlog/spdlog.h>

using namespace DirtSim;

// Treat AIR as a no-flux boundary for pressure diffusion.
// When true, pressure doesn't leak into AIR cells (sealed boundaries).
// When false, AIR participates in diffusion like any other material.
static constexpr bool TREAT_AIR_AS_BOUNDARY = false;

void WorldPressureCalculator::injectGravityPressure(World& world, double deltaTime)
{
    WorldData& data = world.getData();
    const PhysicsSettings& settings = world.getPhysicsSettings();
    const double gravity_magnitude = std::abs(settings.gravity);

    if (gravity_magnitude < 0.0001) {
        return;
    }

    const double hydrostatic_strength = settings.pressure_hydrostatic_strength;

    // Each cell pushes its weight onto the cell below.
    // Process top-to-bottom so pressure accumulates naturally.
    for (uint32_t x = 0; x < data.width; ++x) {
        for (uint32_t y = 0; y < data.height - 1; ++y) {
            Cell& cell = data.at(x, y);

            if (cell.isEmpty() || cell.isWall()) {
                continue;
            }

            // All materials contribute to pressure based on pressure_injection_weight.
            // This creates correct buoyancy gradients for lighter materials in heavier fluids.
            const MaterialProperties& props = getMaterialProperties(cell.material_type);

            // Skip if material doesn't inject pressure (e.g., WALL).
            if (props.pressure_injection_weight <= 0.0) {
                continue;
            }

            Cell& below = data.at(x, y + 1);
            if (below.isWall()) {
                continue;
            }

            // Inject pressure: weight = density * gravity * injection_weight.
            double weight = cell.getEffectiveDensity() * gravity_magnitude;
            double pressure_contribution =
                weight * props.pressure_injection_weight * hydrostatic_strength * deltaTime;

            below.pressure += pressure_contribution;
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
    // Cache data reference.
    WorldData& data = world.getData();

    // Process each blocked transfer.
    for (const auto& transfer : blocked_transfers) {
        // Determine where to apply pressure.
        bool apply_to_target = false;

        // Check if target cell is valid for pressure transmission.
        if (isValidCell(world, transfer.toX, transfer.toY)) {
            Cell& target_cell = data.at(transfer.toX, transfer.toY);

            // Check target cell type.
            if (target_cell.isWall()) {
                // Walls reflect pressure back to source.
                if (isValidCell(world, transfer.fromX, transfer.fromY)) {
                    Cell& source_cell = data.at(transfer.fromX, transfer.fromY);

                    // Get material-specific dynamic weight for source.
                    const double material_weight =
                        getMaterialProperties(source_cell.material_type).dynamic_weight;
                    double dynamic_strength = world.getPhysicsSettings().pressure_dynamic_strength;

                    // Calculate material-based reflection coefficient.
                    double reflection_coefficient =
                        calculateReflectionCoefficient(source_cell.material_type, transfer.energy);

                    double reflected_energy = transfer.energy * material_weight * dynamic_strength
                        * reflection_coefficient;

                    source_cell.pressure += reflected_energy;

                    spdlog::debug(
                        "Blocked transfer from ({},{}) to WALL at ({},{}): amount={:.3f}, "
                        "energy={:.3f}, "
                        "reflecting to SOURCE cell with material={}, weight={:.2f}, "
                        "reflection_coeff={:.2f}",
                        transfer.fromX,
                        transfer.fromY,
                        transfer.toX,
                        transfer.toY,
                        transfer.transfer_amount,
                        transfer.energy,
                        getMaterialName(source_cell.material_type),
                        material_weight,
                        reflection_coefficient);
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
            Cell& target_cell = data.at(transfer.toX, transfer.toY);

            double material_weight =
                getMaterialProperties(target_cell.material_type).dynamic_weight;
            double dynamic_strength = world.getPhysicsSettings().pressure_dynamic_strength;
            double weighted_energy = blocked_energy * material_weight * dynamic_strength;

            target_cell.pressure += weighted_energy;

            spdlog::debug(
                "Blocked transfer from ({},{}) to ({},{}): amount={:.3f}, energy={:.3f}, "
                "applying to TARGET cell with material={}, weight={:.2f}",
                transfer.fromX,
                transfer.fromY,
                transfer.toX,
                transfer.toY,
                transfer.transfer_amount,
                blocked_energy,
                getMaterialName(target_cell.material_type),
                material_weight);
        }
    }
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

    // Cache data reference.
    const WorldData& data = world.getData();

    const Cell& center = data.at(x, y);
    double center_pressure = center.pressure;

    Vector2d gradient(0, 0);

    // Calculate HORIZONTAL gradient (∂P/∂x) using left and right neighbors.
    {
        double p_left = center_pressure; // Default to center if no neighbor.
        double p_right = center_pressure;
        bool has_left = false;
        bool has_right = false;

        // Left neighbor.
        if (x > 0) {
            const Cell& left = data.at(x - 1, y);
            if (!left.isWall()) {
                p_left = left.pressure; // Use actual pressure (0 for empty cells).
                has_left = true;
            }
        }

        // Right neighbor.
        if (x < data.width - 1) {
            const Cell& right = data.at(x + 1, y);
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
            const Cell& up = data.at(x, y - 1);
            if (!up.isWall()) {
                p_up = up.pressure; // Use actual pressure (0 for empty cells).
                has_up = true;
            }
        }

        // Down neighbor.
        if (y < data.height - 1) {
            const Cell& down = data.at(x, y + 1);
            if (!down.isWall()) {
                p_down = down.pressure; // Use actual pressure (0 for empty cells).
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
    // Cache data reference.
    const WorldData& data = world.getData();
    const Cell& center = data.at(x, y);
    double center_density = center.getEffectiveDensity();

    // Get gravity vector and magnitude.
    Vector2d gravity = Vector2d(0, world.getPhysicsSettings().gravity);
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
            const Cell& neighbor = data.at(nx, ny);

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
    WorldData& data = world.getData();
    const PhysicsSettings& settings = world.getPhysicsSettings();

    for (uint32_t y = 0; y < data.height; ++y) {
        for (uint32_t x = 0; x < data.width; ++x) {
            Cell& cell = data.at(x, y);

            if (cell.pressure > MIN_PRESSURE_THRESHOLD) {
                cell.pressure *= (1.0 - settings.pressure_decay_rate * deltaTime);
            }

            // Update pressure gradient for visualization.
            if (cell.fill_ratio >= MIN_MATTER_THRESHOLD && !cell.isWall()) {
                if (cell.pressure >= MIN_PRESSURE_THRESHOLD) {
                    cell.pressure_gradient = calculatePressureGradient(world, x, y);
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
    WorldData& data = world.getData();
    const Vector2d gravity = Vector2d(0, world.getPhysicsSettings().gravity);
    const double gravity_magnitude = gravity.magnitude();

    if (gravity_magnitude < 0.0001) {
        return;
    }

    // Process all cells to generate virtual gravity transfers.
    for (uint32_t y = 0; y < data.height; ++y) {
        for (uint32_t x = 0; x < data.width; ++x) {
            Cell& cell = data.at(x, y);

            // Skip empty cells and walls.
            if (cell.fill_ratio < MIN_MATTER_THRESHOLD || cell.isWall()) {
                continue;
            }

            // Calculate virtual downward velocity from gravity.
            Vector2d gravity_velocity = gravity * deltaTime;

            // Calculate virtual force from gravity, scaled by deltaTime for pressure contribution.
            // Using force-based approach (F = m*g) rather than kinetic energy (½mv²) for stability.
            // This is linear in deltaTime, avoiding Δt² instability with variable timesteps.
            double virtual_force = cell.getEffectiveDensity() * gravity_magnitude;
            double virtual_energy = virtual_force * deltaTime;

            // Check if downward motion would be blocked.
            // For now, assume gravity points down (0, 1).
            int below_x = x;
            int below_y = y + 1;

            bool would_be_blocked = false;
            if (isValidCell(world, below_x, below_y)) {
                const Cell& cell_below = data.at(below_x, below_y);
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
    // Cache data reference.
    const WorldData& data = world.getData();

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
        if (nx < 0 || nx >= static_cast<int>(data.width) || ny < 0
            || ny >= static_cast<int>(data.height)) {
            continue; // Out of bounds.
        }

        const Cell& neighbor = data.at(nx, ny);

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
    // Cache world data and settings to avoid repeated pImpl dereferences.
    WorldData& data = world.getData(); // Cached
    const PhysicsSettings& settings = world.getPhysicsSettings();
    const uint32_t width = data.width;
    const uint32_t height = data.height;
    std::vector<double> new_pressure(width * height);

    // Copy current pressure values.
    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            size_t idx = y * width + x;
            const Cell& cell = data.at(x, y);
            new_pressure[idx] = cell.pressure;
        }
    }

    constexpr bool USE_8_NEIGHBORS = true;
    const int num_iterations = std::max(1, settings.pressure_diffusion_iterations);

    for (int iteration = 0; iteration < num_iterations; ++iteration) {
        std::vector<double> temp_pressure = new_pressure;

        for (uint32_t y = 0; y < height; ++y) {
            for (uint32_t x = 0; x < width; ++x) {
                size_t idx = y * width + x;
                const Cell& cell = data.at(x, y);

                // Skip empty cells and walls.
                if (cell.isEmpty() || cell.material_type == MaterialType::WALL) {
                    continue;
                }

                // Get material diffusion coefficient.
                const MaterialProperties& props = getMaterialProperties(cell.material_type);
                double diffusion_rate =
                    props.pressure_diffusion * settings.pressure_diffusion_strength;

                // Define neighbor offsets based on mode.
                constexpr int dx_8[] = { -1, 0, 1, -1, 1, -1, 0, 1 };
                constexpr int dy_8[] = { -1, -1, -1, 0, 0, 1, 1, 1 };
                constexpr int dx_4[] = { 0, 0, 1, -1 };
                constexpr int dy_4[] = { -1, 1, 0, 0 };

                const int* dx = USE_8_NEIGHBORS ? dx_8 : dx_4;
                const int* dy = USE_8_NEIGHBORS ? dy_8 : dy_4;
                const int num_neighbors = USE_8_NEIGHBORS ? 8 : 4;

                double pressure_flux = 0.0;
                const double current_pressure = temp_pressure[idx];

                for (int i = 0; i < num_neighbors; ++i) {
                    int nx = static_cast<int>(x) + dx[i];
                    int ny = static_cast<int>(y) + dy[i];

                    double neighbor_pressure;
                    double neighbor_diffusion;

                    if (nx < 0 || nx >= static_cast<int>(width) || ny < 0
                        || ny >= static_cast<int>(height)) {
                        neighbor_pressure = current_pressure;
                        neighbor_diffusion = diffusion_rate;
                    }
                    else {
                        const Cell& neighbor = data.at(nx, ny);

                        if (neighbor.material_type == MaterialType::WALL) {
                            neighbor_pressure = current_pressure;
                            neighbor_diffusion = diffusion_rate;
                        }
                        else if (neighbor.isEmpty()) {
                            neighbor_pressure = current_pressure;
                            neighbor_diffusion = diffusion_rate;
                        }
                        else if (
                            TREAT_AIR_AS_BOUNDARY && neighbor.material_type == MaterialType::AIR) {
                            // Treat AIR as no-flux boundary (pressure sealed in).
                            neighbor_pressure = current_pressure;
                            neighbor_diffusion = diffusion_rate;
                        }
                        else {
                            size_t neighbor_idx = ny * width + nx;
                            neighbor_pressure = temp_pressure[neighbor_idx];
                            neighbor_diffusion =
                                getMaterialProperties(neighbor.material_type).pressure_diffusion;
                        }
                    }

                    double pressure_diff = neighbor_pressure - current_pressure;

                    double interface_diffusion = 2.0 * diffusion_rate * neighbor_diffusion
                        / (diffusion_rate + neighbor_diffusion + 1e-10);

                    if (USE_8_NEIGHBORS && dx[i] != 0 && dy[i] != 0) {
                        interface_diffusion *= 0.707107;
                    }

                    pressure_flux += interface_diffusion * pressure_diff;
                }

                // Update pressure with diffusion flux.
                // Scale by deltaTime for frame-rate independence.
                // Apply stability limiter to prevent numerical instability.
                double pressure_change = pressure_flux * deltaTime;

                new_pressure[idx] = current_pressure + pressure_change;

                // Ensure pressure doesn't go negative.
                if (new_pressure[idx] < 0.0) {
                    new_pressure[idx] = 0.0;
                }
            }
        }
    }

    // Apply the new pressure values.
    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            size_t idx = y * width + x;
            Cell& cell = data.at(x, y);
            cell.pressure = std::max(0.0, new_pressure[idx]);
        }
    }
}
