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

    // Process each column independently.
    for (uint32_t x = 0; x < world.getWidth(); ++x) {
        // Phase 1: Bottom-up support detection.
        bool has_support_below = true; // Bottom boundary provides support.
        std::vector<bool> cell_has_support(world.getHeight());

        for (int y = world.getHeight() - 1; y >= 0; --y) {
            Cell& cell = world.at(x, y);

            if (cell.isEmpty()) {
                // Empty cells break support chain.
                has_support_below = false;
            }
            else if (cell.isWall() || isRigidSupport(cell.material_type)) {
                // Solid materials restore support.
                has_support_below = true;
            }
            // else: fluid materials inherit support from below.

            // Record support state for this cell.
            cell_has_support[y] = !cell.isEmpty() && has_support_below;
        }

        // Phase 2: Top-down pressure accumulation.
        double accumulated_pressure = 0.0;

        for (uint32_t y = 0; y < world.getHeight(); ++y) {
            Cell& cell = world.at(x, y);

            if (cell.isEmpty()) {
                // Empty cells: reset accumulation, no pressure.
                accumulated_pressure = 0.0;
                continue;
            }

            if (cell_has_support[y]) {
                // Supported cells receive accumulated pressure.
                double current_pressure = cell.pressure;
                cell.pressure = current_pressure + accumulated_pressure;
                cell.setHydrostaticPressure(accumulated_pressure);

                // Add this cell's contribution for cells below.
                double effective_density = cell.getEffectiveDensity();
                double hydrostatic_weight = getHydrostaticWeight(cell.material_type);
                accumulated_pressure += effective_density * hydrostatic_weight * gravity_magnitude
                    * SLICE_THICKNESS * hydrostatic_strength;
            }
            else {
                // Unsupported cells: no hydrostatic pressure.
                cell.setHydrostaticPressure(0.0);
                // Don't accumulate pressure from unsupported cells.
            }
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
                    double reflection_coefficient = calculateReflectionCoefficient(
                        source_cell.material_type, transfer.energy);

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
    // Material-specific hydrostatic pressure sensitivity.
    switch (type) {
        case MaterialType::WATER:
            return 1.0;
        case MaterialType::SAND:
          return 0.7;
        case MaterialType::DIRT:
            return 0.3;
        case MaterialType::WOOD:
            return 0.1;
        case MaterialType::METAL:
            return 0.05;
        case MaterialType::LEAF:
            return 0.3;
        case MaterialType::WALL:
        case MaterialType::AIR:
        default:
            return 0.0; // No hydrostatic response.
    }
}

double WorldPressureCalculator::getDynamicWeight(MaterialType type) const
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

Vector2d WorldPressureCalculator::calculatePressureGradient(const World& world, uint32_t x, uint32_t y) const
{
    // Get center cell total pressure.
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
        return Vector2d{0, 0};
    }

    // First pass: Identify blocked and open directions.
    Vector2d gradient(0, 0);
    double total_blocked_pressure = 0.0;
    int open_directions = 0;
    int wall_directions = 0;

    if (gradient_directions_ == PressureGradientDirections::Four) {
        // Check all 4 cardinal neighbors.
        const std::array<std::pair<int, int>, 4> directions = {
            { { -1, 0 }, { 1, 0 }, { 0, -1 }, { 0, 1 } }
        };

        // First pass: Calculate pressure differences and identify blocked directions.
        std::array<double, 4> pressure_diffs = { 0, 0, 0, 0 };
        std::array<bool, 4> is_blocked = { false, false, false, false };

        for (size_t i = 0; i < directions.size(); ++i) {
            const auto& [dx, dy] = directions[i];
            int nx = static_cast<int>(x) + dx;
            int ny = static_cast<int>(y) + dy;

            double neighbor_pressure;
            bool is_wall = false;

            if (isValidCell(world, nx, ny)) {
                const Cell& neighbor = world.at(nx, ny);

                if (neighbor.isWall()) {
                    is_wall = true;
                    wall_directions++;
                    // Calculate pressure difference as if wall wasn't there.
                    neighbor_pressure = 0.0;
                }
                else if (neighbor.isEmpty()) {
                    // Empty cells act as pressure sinks.
                    neighbor_pressure = 0.0;
                    open_directions++;
                }
                else {
                    // Normal cells use their actual pressure.
                    neighbor_pressure = neighbor.pressure;
                    open_directions++;
                }
            }
            else {
                // Out-of-bounds: treat as wall.
                is_wall = true;
                wall_directions++;
                neighbor_pressure = 0.0;
            }

            pressure_diffs[i] = center_pressure - neighbor_pressure;
            is_blocked[i] = is_wall;

            if (is_wall) {
                // Track blocked pressure that needs redistribution.
                total_blocked_pressure += pressure_diffs[i];
            }

            spdlog::trace(
                "  Direction ({},{}) - neighbor at ({},{}) - pressure={:.6f}, diff={:.6f}, "
                "blocked={}",
                dx,
                dy,
                nx,
                ny,
                neighbor_pressure,
                pressure_diffs[i],
                is_wall ? "YES" : "NO");
        }

        // Second pass: Calculate gradient with redistribution.
        for (size_t i = 0; i < directions.size(); ++i) {
            const auto& [dx, dy] = directions[i];

            if (!is_blocked[i]) {
                // Add direct pressure contribution.
                gradient.x += pressure_diffs[i] * dx;
                gradient.y += pressure_diffs[i] * dy;

                // Add redistributed pressure from blocked directions.
                if (open_directions > 0 && total_blocked_pressure > 0) {
                    double redistribution = total_blocked_pressure / open_directions;
                    gradient.x += redistribution * dx;
                    gradient.y += redistribution * dy;

                    spdlog::trace(
                        "  Redistributing {:.6f} pressure to direction ({},{})",
                        redistribution,
                        dx,
                        dy);
                }
            }
        }
    }
    else { // PressureGradientDirections::Eight
        // Check all 8 neighbors (including diagonals).
        const int dx[] = { -1, 0, 1, -1, 1, -1, 0, 1 };
        const int dy[] = { -1, -1, -1, 0, 0, 1, 1, 1 };
        constexpr int num_neighbors = 8;

        // First pass: Calculate pressure differences and identify blocked directions.
        std::array<double, 8> pressure_diffs = { 0, 0, 0, 0, 0, 0, 0, 0 };
        std::array<bool, 8> is_blocked = { false, false, false, false, false, false, false, false };
        std::array<double, 8> direction_weights = { 1, 1, 1, 1, 1, 1, 1, 1 };

        // Calculate weights for diagonal directions.
        for (int i = 0; i < num_neighbors; ++i) {
            if (dx[i] != 0 && dy[i] != 0) {
                direction_weights[i] = 0.707107; // 1/sqrt(2)
            }
        }

        for (int i = 0; i < num_neighbors; ++i) {
            int nx = static_cast<int>(x) + dx[i];
            int ny = static_cast<int>(y) + dy[i];

            double neighbor_pressure;
            bool is_wall = false;

            if (isValidCell(world, nx, ny)) {
                const Cell& neighbor = world.at(nx, ny);

                if (neighbor.isWall()) {
                    is_wall = true;
                    wall_directions++;
                    // Calculate pressure difference as if wall wasn't there.
                    neighbor_pressure = 0.0;
                }
                else if (neighbor.isEmpty()) {
                    // Empty cells act as pressure sinks.
                    neighbor_pressure = 0.0;
                    open_directions++;
                }
                else {
                    // Normal cells use their actual pressure.
                    neighbor_pressure = neighbor.pressure;
                    open_directions++;
                }
            }
            else {
                // Out-of-bounds: treat as wall.
                is_wall = true;
                wall_directions++;
                neighbor_pressure = 0.0;
            }

            pressure_diffs[i] = center_pressure - neighbor_pressure;
            is_blocked[i] = is_wall;

            if (is_wall) {
                // Track blocked pressure that needs redistribution.
                total_blocked_pressure += pressure_diffs[i] * direction_weights[i];
            }

            spdlog::trace(
                "  Direction ({},{}) - neighbor at ({},{}) - pressure={:.6f}, diff={:.6f}, "
                "weight={:.3f}, blocked={}",
                dx[i],
                dy[i],
                nx,
                ny,
                neighbor_pressure,
                pressure_diffs[i],
                direction_weights[i],
                is_wall ? "YES" : "NO");
        }

        // Second pass: Calculate gradient with redistribution.
        double total_open_weight = 0.0;
        for (int i = 0; i < num_neighbors; ++i) {
            if (!is_blocked[i]) {
                total_open_weight += direction_weights[i];
            }
        }

        for (int i = 0; i < num_neighbors; ++i) {
            if (!is_blocked[i]) {
                // Add direct pressure contribution.
                gradient.x += pressure_diffs[i] * dx[i] * direction_weights[i];
                gradient.y += pressure_diffs[i] * dy[i] * direction_weights[i];

                // Add redistributed pressure from blocked directions.
                if (total_open_weight > 0 && total_blocked_pressure > 0) {
                    double redistribution =
                        (total_blocked_pressure / total_open_weight) * direction_weights[i];
                    gradient.x += redistribution * dx[i];
                    gradient.y += redistribution * dy[i];

                    spdlog::trace(
                        "  Redistributing {:.6f} pressure to direction ({},{})",
                        redistribution,
                        dx[i],
                        dy[i]);
                }
            }
        }
    }

    // Normalize by number of directions checked (not just open directions).
    if (gradient_directions_ == PressureGradientDirections::Four) {
        gradient = gradient / 4.0;
    }
    else {
        gradient = gradient / 8.0;
    }

    spdlog::trace(
        "Pressure gradient at ({},{}) - center_pressure={:.6f}, "
        "gradient=({:.6f},{:.6f}), open_dirs={}, wall_dirs={}, "
        "blocked_pressure={:.6f} (mode: {})",
        x,
        y,
        center_pressure,
        gradient.x,
        gradient.y,
        open_directions,
        wall_directions,
        total_blocked_pressure,
        gradient_directions_ == PressureGradientDirections::Four ? "4-dir" : "8-dir");

    return gradient;
}

Vector2d WorldPressureCalculator::calculateGravityGradient(const World& world, uint32_t x, uint32_t y) const
{
    const Cell& center = world.at(x, y);
    double center_density = center.getEffectiveDensity();

    // Get gravity vector and magnitude.
    Vector2d gravity = world.getGravityVector();
    double gravity_magnitude = gravity.magnitude();

    // Skip if no gravity.
    if (gravity_magnitude < 0.001) {
        return Vector2d{0, 0};
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
    // Apply decay to unified pressure values.
    for (uint32_t y = 0; y < world.getHeight(); ++y) {
        for (uint32_t x = 0; x < world.getWidth(); ++x) {
            Cell& cell = world.at(x, y);

            // Apply pressure decay to the unified pressure.
            double pressure = cell.pressure;
            if (pressure > MIN_PRESSURE_THRESHOLD) {
                double new_pressure = pressure * (1.0 - DYNAMIC_DECAY_RATE * deltaTime);
                cell.pressure = new_pressure;

                // Update components proportionally for visualization.
                double decay_factor = new_pressure / pressure;
                cell.hydrostatic_component = cell.hydrostatic_component * decay_factor;
                cell.dynamic_component = cell.dynamic_component * decay_factor;
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
                    cell.pressure_gradient = Vector2d{0.0, 0.0};
                }
            }
            else {
                cell.pressure_gradient = Vector2d{0.0, 0.0};
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
    for (uint32_t y = 0; y < world.getHeight(); ++y) {
        for (uint32_t x = 0; x < world.getWidth(); ++x) {
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

double WorldPressureCalculator::calculateDiffusionReflectionCoefficient(
    MaterialType material, double blocked_flux) const
{
    // Base reflection depends on material properties.
    const MaterialProperties& props = getMaterialProperties(material);

    // Materials with higher elasticity reflect more pressure.
    // Materials with lower density (like air) reflect less.
    double base_reflection = 0.7 * props.elasticity + 0.3 * (1.0 - props.density / 10.0);

    // Reduce reflection for very small pressure differences (damping).
    // This prevents numerical noise from building up.
    double flux_damping = 1.0 - std::exp(-blocked_flux * 10.0);

    double reflection_coefficient = base_reflection * flux_damping;

    spdlog::trace(
        "Diffusion reflection coefficient for {}: elasticity={:.2f}, density={:.1f}, "
        "base_reflection={:.3f}, flux={:.6f}, damping={:.3f}, final={:.3f}",
        getMaterialName(material),
        props.elasticity,
        props.density,
        base_reflection,
        blocked_flux,
        flux_damping,
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

void WorldPressureCalculator::applyPressureDiffusion(World& world, double deltaTime)
{
    // Create a temporary copy of pressure values to avoid order-dependent updates.
    const uint32_t width = world.getWidth();
    const uint32_t height = world.getHeight();
    std::vector<double> new_pressure(width * height);

    // Initialize wall reflection tracking.
    wall_reflections_.clear();
    wall_reflections_.resize(height, std::vector<WallReflection>(width));

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
            double diffusion_rate = props.pressure_diffusion;

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

                        // Walls reflect pressure back.
                        if (neighbor.material_type == MaterialType::WALL) {
                            // Skip harmonic mean for walls, use cell's diffusion rate directly.
                            double interface_diffusion_wall =
                                diffusion_rate * WALL_INTERFACE_FACTOR;

                            // For diagonal neighbors, scale by 1/sqrt(2) to account for distance.
                            if (dx[i] != 0 && dy[i] != 0) {
                                interface_diffusion_wall *= 0.707107; // 1/sqrt(2)
                            }

                            // Calculate potential flux based on pressure difference.
                            // Assume wall would have half the current pressure if it wasn't solid.
                            double assumed_wall_pressure = current_pressure * 0;
                            double pressure_diff = current_pressure - assumed_wall_pressure;
                            double potential_flux = interface_diffusion_wall * pressure_diff;

                            // Track this blocked flux for reflection.
                            wall_reflections_[y][x].blocked_flux += potential_flux;
                            wall_reflections_[y][x].reflection_count++;
                            continue;
                        }

                        // Empty cells act as pressure sinks.
                        neighbor_pressure = neighbor.isEmpty() ? 0.0 : neighbor.pressure;
                        neighbor_diffusion = neighbor.isEmpty()
                            ? 1.0
                            : getMaterialProperties(neighbor.material_type).pressure_diffusion;
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

                        // Walls reflect pressure back.
                        if (neighbor.material_type == MaterialType::WALL) {
                            // Skip harmonic mean for walls, use cell's diffusion rate directly.
                            double interface_diffusion_wall =
                                diffusion_rate * WALL_INTERFACE_FACTOR;

                            // No diagonal scaling needed in 4-neighbor case.

                            // Calculate potential flux based on pressure difference.
                            // Assume wall would have half the current pressure if it wasn't solid.
                            double assumed_wall_pressure = current_pressure * 0.5;
                            double pressure_diff = current_pressure - assumed_wall_pressure;
                            double potential_flux = interface_diffusion_wall * pressure_diff;

                            // Track this blocked flux for reflection.
                            wall_reflections_[y][x].blocked_flux += potential_flux;
                            wall_reflections_[y][x].reflection_count++;
                            continue;
                        }

                        // Empty cells act as pressure sinks.
                        neighbor_pressure = neighbor.isEmpty() ? 0.0 : neighbor.pressure;
                        neighbor_diffusion = neighbor.isEmpty()
                            ? 1.0
                            : getMaterialProperties(neighbor.material_type).pressure_diffusion;
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
            double max_change = current_pressure * 0.5 + 0.1; // Max 50% change + small absolute floor.
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

    // Apply wall reflections before finalizing pressure values.
    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            // Skip cells with no blocked flux.
            if (wall_reflections_[y][x].blocked_flux <= 0.0) {
                continue;
            }

            size_t idx = y * width + x;
            const Cell& cell = world.at(x, y);

            // Calculate reflection coefficient based on material.
            double reflection_coeff = calculateDiffusionReflectionCoefficient(
                cell.material_type, wall_reflections_[y][x].blocked_flux);

            // Apply reflected pressure.
            double reflected_pressure = wall_reflections_[y][x].blocked_flux * reflection_coeff;
            new_pressure[idx] += reflected_pressure * deltaTime;

            // Ensure pressure stays positive.
            if (new_pressure[idx] < 0.0) {
                new_pressure[idx] = 0.0;
            }

            spdlog::trace(
                "Pressure reflection at ({},{}) - material={}, blocked_flux={:.6f}, "
                "reflection_coeff={:.3f}, added_pressure={:.6f}",
                x,
                y,
                getMaterialName(cell.material_type),
                wall_reflections_[y][x].blocked_flux,
                reflection_coeff,
                reflected_pressure * deltaTime);
        }
    }

    // Apply the new pressure values.
    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            size_t idx = y * width + x;
            Cell& cell = world.at(x, y);
            double old_pressure = cell.pressure;
            double new_unified_pressure = new_pressure[idx];
            cell.pressure = new_unified_pressure;

            // Update components proportionally based on diffusion.
            if (old_pressure > 0.0) {
                double ratio = new_unified_pressure / old_pressure;
                cell.hydrostatic_component = cell.hydrostatic_component * ratio;
                cell.dynamic_component = cell.dynamic_component * ratio;
            }
        }
    }
}
