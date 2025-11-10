#include "World.h"
#include "Cell.h"
#include "ReflectSerializer.h"
#include "ScopeTimer.h"
#include "Vector2i.h"
#include "WorldAirResistanceCalculator.h"
#include "WorldCohesionCalculator.h"
#include "WorldCollisionCalculator.h"
#include "WorldDiagramGeneratorEmoji.h"
#include "WorldInterpolationTool.h"
#include "WorldSupportCalculator.h"
#include "spdlog/spdlog.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <queue>
#include <random>
#include <set>
#include <sstream>

namespace DirtSim {

World::World() : World(1, 1)
{}

World::World(uint32_t width, uint32_t height)
    : water_pressure_threshold_(0.0004),
      cohesion_bind_force_enabled_(false),
      cohesion_com_force_strength_(0.0),
      cohesion_bind_force_strength_(1.0),
      com_cohesion_range_(1),
      viscosity_strength_(1.0),
      friction_strength_(1.0),
      air_resistance_enabled_(true),
      air_resistance_strength_(0.1),
      selected_material_(MaterialType::DIRT),
      support_calculator_(),
      pressure_calculator_(),
      collision_calculator_(),
      adhesion_calculator_(),
      friction_calculator_(),
      worldEventGenerator_(nullptr)
{
    // Set dimensions (other WorldData members use defaults from struct declaration).
    data.width = width;
    data.height = height;

    spdlog::info("Creating World: {}x{} grid with pure-material physics", data.width, data.height);

    // Initialize cell grid.
    data.cells.resize(data.width * data.height);

    // Initialize with air.
    for (auto& cell : data.cells) {
        cell = Cell{ MaterialType::AIR, 0.0 };
    }

    // Set up boundary walls if enabled.
    if (areWallsEnabled()) {
        setupBoundaryWalls();
    }

    timers_.startTimer("total_simulation");

    spdlog::info("World initialization complete");
}

World::~World()
{
    spdlog::info("Destroying World: {}x{} grid", data.width, data.height);
    timers_.stopTimer("total_simulation");
    timers_.dumpTimerStats();
}

std::string World::toAsciiDiagram() const
{
    return WorldDiagramGeneratorEmoji::generateEmojiDiagram(*this);
}

// =================================================================
// CORE SIMULATION METHODS
// =================================================================

void World::advanceTime(double deltaTimeSeconds)
{
    ScopeTimer timer(timers_, "advance_time");

    const double scaledDeltaTime = deltaTimeSeconds * physicsSettings.timescale;
    spdlog::trace(
        "World::advanceTime: deltaTime={:.4f}s, timestep={}", deltaTimeSeconds, data.timestep);
    if (scaledDeltaTime <= 0.0) {
        return;
    }

    // Add particles if enabled.
    if (data.add_particles_enabled && worldEventGenerator_) {
        ScopeTimer addParticlesTimer(timers_, "add_particles");
        worldEventGenerator_->addParticles(*this, data.timestep, deltaTimeSeconds);
    }

    // Pre-compute support map for all cells (bottom-up pass).
    {
        ScopeTimer supportMapTimer(timers_, "compute_support_map");
        WorldSupportCalculator support_calc{};
        support_calc.computeSupportMapBottomUp(*this);
    }

    // Accumulate and apply all forces based on resistance.
    // This now includes pressure forces from the previous frame.
    {
        ScopeTimer resolveTimer(timers_, "resolve_forces_total");
        resolveForces(scaledDeltaTime);
    }

    {
        ScopeTimer velocityTimer(timers_, "velocity_limiting");
        processVelocityLimiting(scaledDeltaTime);
    }

    {
        ScopeTimer transfersTimer(timers_, "update_transfers");
        updateTransfers(scaledDeltaTime);
    }

    // Process queued material moves - this detects NEW blocked transfers.
    {
        ScopeTimer movesTimer(timers_, "process_moves_total");
        processMaterialMoves();
    }

    // Calculate pressures for NEXT frame after moves are complete.
    // This follows the two-frame model where pressure calculated in frame N
    // affects velocities in frame N+1.
    if (physicsSettings.pressure_hydrostatic_strength > 0.0) {
        ScopeTimer hydroTimer(timers_, "hydrostatic_pressure");
        pressure_calculator_.calculateHydrostaticPressure(*this);
    }

    // Process any blocked transfers that were queued during processMaterialMoves.
    if (physicsSettings.pressure_dynamic_strength > 0.0) {
        ScopeTimer dynamicTimer(timers_, "dynamic_pressure");
        // Generate virtual gravity transfers to create pressure from gravity forces.
        // This allows dynamic pressure to model hydrostatic-like behavior.
        //        pressure_calculator_.generateVirtualGravityTransfers(scaledDeltaTime);

        pressure_calculator_.processBlockedTransfers(
            *this, pressure_calculator_.blocked_transfers_);
        pressure_calculator_.blocked_transfers_.clear();
    }

    // Apply pressure diffusion before decay.
    if (physicsSettings.pressure_diffusion_strength > 0.0) {
        ScopeTimer diffusionTimer(timers_, "pressure_diffusion");
        pressure_calculator_.applyPressureDiffusion(*this, scaledDeltaTime);
    }

    // Apply pressure decay after material moves.
    {
        ScopeTimer decayTimer(timers_, "pressure_decay");
        pressure_calculator_.applyPressureDecay(*this, scaledDeltaTime);
    }

    data.timestep++;
}
void World::reset()
{
    spdlog::info("Resetting World to empty state");

    data.timestep = 0;
    data.removed_mass = 0.0;
    pending_moves_.clear();

    // Clear all cells to air.
    for (auto& cell : data.cells) {
        cell.clear();
    }

    spdlog::info("World reset complete - world is now empty");
}

void World::setup()
{
    // Call WorldEventGenerator's setup to create scenario features.
    if (worldEventGenerator_) {
        worldEventGenerator_->setup(*this);
    }

    // World-specific: Rebuild boundary walls if enabled.
    if (areWallsEnabled()) {
        setupBoundaryWalls();
    }

    spdlog::info("World setup complete");
    spdlog::info("DEBUGGING: Total mass after setup = {:.3f}", getTotalMass());
}

// =================================================================.
// MATERIAL ADDITION METHODS.
// =================================================================.

void World::addMaterialAtCell(uint32_t x, uint32_t y, MaterialType type, double amount)
{
    if (!isValidCell(x, y)) {
        return;
    }

    Cell& cell = at(x, y);
    const double added = cell.addMaterial(type, amount);

    if (added > 0.0) {
        spdlog::trace("Added {:.3f} {} at cell ({},{})", added, getMaterialName(type), x, y);
    }
}

void World::addMaterialAtPixel(int pixelX, int pixelY, MaterialType type, double amount)
{
    int cellX, cellY;
    pixelToCell(pixelX, pixelY, cellX, cellY);

    spdlog::debug(
        "World::addMaterialAtPixel({}) at pixel ({},{}) -> cell ({},{})",
        getMaterialName(type),
        pixelX,
        pixelY,
        cellX,
        cellY);

    if (isValidCell(cellX, cellY)) {
        addMaterialAtCell(static_cast<uint32_t>(cellX), static_cast<uint32_t>(cellY), type, amount);
    }
}

// =================================================================.
// GRID MANAGEMENT.
// =================================================================.

void World::resizeGrid(uint32_t newWidth, uint32_t newHeight)
{
    if (!shouldResize(newWidth, newHeight)) {
        return;
    }

    onPreResize(newWidth, newHeight);

    // Phase 1: Generate interpolated cells using the interpolation tool.
    std::vector<Cell> interpolatedCells = WorldInterpolationTool::generateInterpolatedCellsB(
        data.cells, data.width, data.height, newWidth, newHeight);

    // Phase 2: Update world state with the new interpolated cells.
    data.width = newWidth;
    data.height = newHeight;
    data.cells = std::move(interpolatedCells);

    onPostResize();

    spdlog::info("World bilinear resize complete");
}

void World::onPostResize()
{
    // Rebuild boundary walls if enabled.
    if (areWallsEnabled()) {
        setupBoundaryWalls();
    }
}

// =================================================================.
// WORLDB-SPECIFIC METHODS.
// =================================================================.

Cell& World::at(uint32_t x, uint32_t y)
{
    assert(x < data.width && y < data.height);
    return data.cells[coordToIndex(x, y)];
}

const Cell& World::at(uint32_t x, uint32_t y) const
{
    assert(x < data.width && y < data.height);
    return data.cells[coordToIndex(x, y)];
}

Cell& World::at(const Vector2i& pos)
{
    return at(static_cast<uint32_t>(pos.x), static_cast<uint32_t>(pos.y));
}

const Cell& World::at(const Vector2i& pos) const
{
    return at(static_cast<uint32_t>(pos.x), static_cast<uint32_t>(pos.y));
}

double World::getTotalMass() const
{
    double totalMass = 0.0;
    int cellCount = 0;
    int nonEmptyCells = 0;

    for (const auto& cell : data.cells) {
        double cellMass = cell.getMass();
        totalMass += cellMass;
        cellCount++;
        if (cellMass > 0.0) {
            nonEmptyCells++;
        }
    }

    spdlog::info(
        "DEBUGGING: World total mass={:.3f} from {} cells ({} non-empty)",
        totalMass,
        cellCount,
        nonEmptyCells);
    return totalMass;
}

// =================================================================.
// INTERNAL PHYSICS METHODS.
// =================================================================.

void World::applyGravity()
{
    ScopeTimer timer(timers_, "apply_gravity");

    const Vector2d gravityForce(0.0, physicsSettings.gravity);

    for (auto& cell : data.cells) {
        if (!cell.isEmpty() && !cell.isWall()) {
            // Accumulate gravity force instead of applying directly.
            cell.addPendingForce(gravityForce);
        }
    }
}

void World::applyAirResistance()
{
    if (!air_resistance_enabled_) {
        return;
    }

    ScopeTimer timer(timers_, "apply_air_resistance");

    WorldAirResistanceCalculator air_resistance_calculator{};

    for (uint32_t y = 0; y < data.height; ++y) {
        for (uint32_t x = 0; x < data.width; ++x) {
            Cell& cell = at(x, y);

            if (!cell.isEmpty() && !cell.isWall()) {
                Vector2d air_resistance_force = air_resistance_calculator.calculateAirResistance(
                    *this, x, y, air_resistance_strength_);
                cell.addPendingForce(air_resistance_force);
            }
        }
    }
}

void World::applyCohesionForces()
{
    if (cohesion_com_force_strength_ <= 0.0) {
        return;
    }

    ScopeTimer timer(timers_, "apply_cohesion_forces");

    // Create calculators once outside the loop.
    WorldCohesionCalculator cohesion_calc{};

    {
        ScopeTimer cohesionTimer(timers_, "cohesion_calculation");
        for (uint32_t y = 0; y < data.height; ++y) {
            for (uint32_t x = 0; x < data.width; ++x) {
                Cell& cell = at(x, y);

                if (cell.isEmpty() || cell.isWall()) {
                    continue;
                }

                // Calculate COM cohesion force.
                WorldCohesionCalculator::COMCohesionForce com_cohesion =
                    cohesion_calc.calculateCOMCohesionForce(*this, x, y, com_cohesion_range_);

                // COM cohesion force accumulation (only if force is active).
                if (com_cohesion.force_active) {
                    Vector2d com_cohesion_force = com_cohesion.force_direction
                        * com_cohesion.force_magnitude * cohesion_com_force_strength_;
                    cell.addPendingForce(com_cohesion_force);
                }
            }
        }
    }

    // Adhesion force accumulation (only if enabled).
    if (adhesion_calculator_.isAdhesionEnabled()) {
        ScopeTimer adhesionTimer(timers_, "adhesion_calculation");
        for (uint32_t y = 0; y < data.height; ++y) {
            for (uint32_t x = 0; x < data.width; ++x) {
                Cell& cell = at(x, y);

                if (cell.isEmpty() || cell.isWall()) {
                    continue;
                }

                WorldAdhesionCalculator::AdhesionForce adhesion =
                    adhesion_calculator_.calculateAdhesionForce(*this, x, y);
                Vector2d adhesion_force = adhesion.force_direction * adhesion.force_magnitude
                    * adhesion_calculator_.getAdhesionStrength();
                cell.addPendingForce(adhesion_force);
            }
        }
    }
}

void World::applyPressureForces()
{
    if (physicsSettings.pressure_hydrostatic_strength <= 0.0 && physicsSettings.pressure_dynamic_strength <= 0.0) {
        return;
    }

    ScopeTimer timer(timers_, "apply_pressure_forces");

    // Apply pressure forces through the pending force system.
    for (uint32_t y = 0; y < data.height; ++y) {
        for (uint32_t x = 0; x < data.width; ++x) {
            Cell& cell = at(x, y);

            // Skip empty cells and walls.
            if (cell.isEmpty() || cell.isWall()) {
                continue;
            }

            // Get total pressure for this cell.
            double total_pressure = cell.pressure;
            if (total_pressure < MIN_MATTER_THRESHOLD) {
                continue;
            }

            // Calculate pressure gradient to determine force direction.
            // The gradient is calculated as (center_pressure - neighbor_pressure) * direction,
            // which points AWAY from high pressure regions (toward increasing pressure).
            Vector2d gradient = pressure_calculator_.calculatePressureGradient(*this, x, y);

            // Only apply force if system is out of equilibrium.
            if (gradient.magnitude() > 0.001) {
                Vector2d pressure_force = gradient * physicsSettings.pressure_scale;
                cell.addPendingForce(pressure_force);

                spdlog::debug(
                    "Cell ({},{}) pressure force: total_pressure={:.4f}, "
                    "gradient=({:.4f},{:.4f}), force=({:.4f},{:.4f})",
                    x,
                    y,
                    total_pressure,
                    gradient.x,
                    gradient.y,
                    pressure_force.x,
                    pressure_force.y);
            }
        }
    }
}

double World::getMotionStateMultiplier(MotionState state, double sensitivity) const
{
    double base_multiplier = 1.0; // Default to STATIC.
    switch (state) {
        case MotionState::STATIC:
            base_multiplier = 1.0;
            break;
        case MotionState::FALLING:
            base_multiplier = 0.3;
            break;
        case MotionState::TURBULENT:
            base_multiplier = 0.1;
            break;
        case MotionState::SLIDING:
            base_multiplier = 0.5;
            break;
    }

    // Interpolate based on sensitivity.
    return 1.0 - sensitivity * (1.0 - base_multiplier);
}

void World::resolveForces(double deltaTime)
{
    ScopeTimer timer(timers_, "resolve_forces");

    // Clear pending forces at the start of each physics frame.
    for (auto& cell : data.cells) {
        cell.clearPendingForce();
    }

    // Apply gravity forces.
    applyGravity();

    // Apply air resistance forces.
    applyAirResistance();

    // Apply pressure forces from previous frame.
    applyPressureForces();

    // Apply cohesion and adhesion forces.
    applyCohesionForces();

    // Apply contact-based friction forces.
    friction_calculator_.calculateAndApplyFrictionForces(*this, deltaTime);

    // Now resolve all accumulated forces using viscosity model.
    for (uint32_t y = 0; y < data.height; ++y) {
        for (uint32_t x = 0; x < data.width; ++x) {
            Cell& cell = at(x, y);

            if (cell.isEmpty() || cell.isWall()) {
                continue;
            }

            // Get the total pending force (net driving force).
            Vector2d net_driving_force = cell.pending_force;

            // Get material properties.
            const MaterialProperties& props = getMaterialProperties(cell.material_type);

            // Use pre-computed support from bottom-up scan.
            double support_factor = cell.has_support ? 1.0 : 0.0;

            // For now, assume STATIC motion state when supported, FALLING otherwise.
            // TODO: Implement proper motion state detection.
            MotionState motion_state =
                support_factor > 0.5 ? MotionState::STATIC : MotionState::FALLING;

            // Calculate motion state multiplier.
            double motion_multiplier =
                getMotionStateMultiplier(motion_state, props.motion_sensitivity);

            // Calculate velocity-dependent friction coefficient.
            double velocity_magnitude = cell.velocity.magnitude();
            double friction_coefficient = getFrictionCoefficient(velocity_magnitude, props);

            // Apply global friction strength multiplier.
            friction_coefficient = 1.0 + (friction_coefficient - 1.0) * friction_strength_;

            // Cache the friction coefficient for visualization.
            cell.cached_friction_coefficient = friction_coefficient;

            // Combine viscosity with friction and motion state.
            double effective_viscosity =
                props.viscosity * friction_coefficient * motion_multiplier * 1000;

            // Apply continuous damping with friction (no thresholds).
            double damping_factor = 1.0
                + (effective_viscosity * viscosity_strength_ * cell.fill_ratio * support_factor);

            // Ensure damping factor is never zero or negative to prevent division by zero.
            if (damping_factor <= 0.0) {
                damping_factor = 0.001; // Minimal damping to prevent division by zero.
            }

            // Store damping info for visualization (X=friction_coefficient, Y=damping_factor).
            // Only store if viscosity is actually enabled and having an effect.
            if (viscosity_strength_ > 0.0 && props.viscosity > 0.0) {
                cell.accumulated_cohesion_force = Vector2d{ friction_coefficient, damping_factor };
            }
            else {
                cell.accumulated_cohesion_force = {}; // Clear when viscosity is off.
            }

            // Calculate velocity change from forces.
            Vector2d velocity_change = net_driving_force / damping_factor * deltaTime;

            // Update velocity.
            Vector2d new_velocity = cell.velocity + velocity_change;
            cell.velocity = new_velocity;

            // Debug logging.
            if (net_driving_force.mag() > 0.001) {
                spdlog::debug(
                    "Cell ({},{}) {} - Force: ({:.3f},{:.3f}), viscosity: {:.3f}, "
                    "friction: {:.3f}, support: {:.1f}, motion_mult: {:.3f}, damping: {:.3f}, "
                    "vel_change: ({:.3f},{:.3f}), new_vel: ({:.3f},{:.3f})",
                    x,
                    y,
                    getMaterialName(cell.material_type),
                    net_driving_force.x,
                    net_driving_force.y,
                    props.viscosity,
                    friction_coefficient,
                    support_factor,
                    motion_multiplier,
                    damping_factor,
                    velocity_change.x,
                    velocity_change.y,
                    new_velocity.x,
                    new_velocity.y);
            }
        }
    }
}

void World::processVelocityLimiting(double deltaTime)
{
    for (auto& cell : data.cells) {
        if (!cell.isEmpty()) {
            cell.limitVelocity(
                MAX_VELOCITY_PER_TIMESTEP,
                VELOCITY_DAMPING_THRESHOLD_PER_TIMESTEP,
                VELOCITY_DAMPING_FACTOR_PER_TIMESTEP,
                deltaTime);
        }
    }
}

void World::updateTransfers(double deltaTime)
{
    ScopeTimer timer(timers_, "update_transfers");

    // Clear previous moves.
    pending_moves_.clear();

    // Compute material moves based on COM positions and velocities.
    pending_moves_ = computeMaterialMoves(deltaTime);
}

std::vector<MaterialMove> World::computeMaterialMoves(double deltaTime)
{
    std::vector<MaterialMove> moves;

    for (uint32_t y = 0; y < data.height; ++y) {
        for (uint32_t x = 0; x < data.width; ++x) {
            Cell& cell = at(x, y);

            if (cell.isEmpty() || cell.isWall()) {
                continue;
            }

            // PHASE 2: Force-Based Movement Threshold.
            // Calculate cohesion and adhesion forces before movement decisions.
            WorldCohesionCalculator::CohesionForce cohesion;
            if (cohesion_bind_force_enabled_) {
                WorldCohesionCalculator cohesion_calc{};
                cohesion = cohesion_calc.calculateCohesionForce(*this, x, y);
            }
            else {
                cohesion = { 0.0, 0 }; // No cohesion resistance when disabled.
            }
            WorldAdhesionCalculator::AdhesionForce adhesion =
                adhesion_calculator_.calculateAdhesionForce(*this, x, y);

            // NEW: Calculate COM-based cohesion force.
            WorldCohesionCalculator::COMCohesionForce com_cohesion;
            if (cohesion_com_force_strength_ > 0.0) {
                WorldCohesionCalculator com_cohesion_calc{};
                com_cohesion =
                    com_cohesion_calc.calculateCOMCohesionForce(*this, x, y, com_cohesion_range_);
            }
            else {
                com_cohesion = {
                    { 0.0, 0.0 }, 0.0, { 0.0, 0.0 }, 0, 0.0, 0.0, false
                }; // No COM cohesion when disabled.
            }

            // Apply strength multipliers to forces.
            double effective_adhesion_magnitude =
                adhesion.force_magnitude * adhesion_calculator_.getAdhesionStrength();
            double effective_com_cohesion_magnitude =
                com_cohesion.force_magnitude * cohesion_com_force_strength_;

            // Store forces in cell for visualization.
            // Note: Cohesion force field is now repurposed in resolveForces() for damping info.
            cell.accumulated_adhesion_force =
                adhesion.force_direction * effective_adhesion_magnitude;
            cell.accumulated_com_cohesion_force =
                com_cohesion.force_direction * effective_com_cohesion_magnitude;

            // NOTE: Force calculation and resistance checking now handled in resolveForces().
            // This method only needs to handle material movement based on COM positions.

            // Debug: Check if cell has any velocity or interesting COM.
            Vector2d current_velocity = cell.velocity;
            Vector2d oldCOM = cell.com;
            if (current_velocity.length() > 0.01 || std::abs(oldCOM.x) > 0.5
                || std::abs(oldCOM.y) > 0.5) {
                spdlog::debug(
                    "Cell ({},{}) {} - Velocity: ({:.3f},{:.3f}), COM: ({:.3f},{:.3f})",
                    x,
                    y,
                    getMaterialName(cell.material_type),
                    current_velocity.x,
                    current_velocity.y,
                    oldCOM.x,
                    oldCOM.y);
            }

            // Update COM based on velocity (with proper deltaTime integration).
            Vector2d newCOM = cell.com + cell.velocity * deltaTime;

            // Enhanced: Check if COM crosses any boundary [-1,1] for universal collision detection.
            std::vector<Vector2i> crossed_boundaries =
                collision_calculator_.getAllBoundaryCrossings(newCOM);

            if (!crossed_boundaries.empty()) {
                spdlog::debug(
                    "Boundary crossings detected for {} at ({},{}) with COM ({:.2f},{:.2f}) -> {} "
                    "crossings",
                    getMaterialName(cell.material_type),
                    x,
                    y,
                    newCOM.x,
                    newCOM.y,
                    crossed_boundaries.size());
            }

            bool boundary_reflection_applied = false;

            for (const Vector2i& direction : crossed_boundaries) {
                Vector2i targetPos = Vector2i(x, y) + direction;

                if (isValidCell(targetPos)) {
                    // Create enhanced MaterialMove with collision physics and COM cohesion data.
                    MaterialMove move = collision_calculator_.createCollisionAwareMove(
                        *this,
                        cell,
                        at(targetPos),
                        Vector2i(x, y),
                        targetPos,
                        direction,
                        deltaTime,
                        com_cohesion);

                    // Debug logging for collision detection.
                    if (move.collision_type != CollisionType::TRANSFER_ONLY) {
                        spdlog::debug(
                            "Collision detected: {} vs {} at ({},{}) -> ({},{}) - Type: {}, "
                            "Energy: {:.3f}",
                            getMaterialName(move.material),
                            getMaterialName(at(targetPos).material_type),
                            x,
                            y,
                            targetPos.x,
                            targetPos.y,
                            static_cast<int>(move.collision_type),
                            move.collision_energy);
                    }

                    moves.push_back(move);
                }
                else {
                    // Hit world boundary - apply elastic reflection immediately.
                    spdlog::debug(
                        "World boundary hit: {} at ({},{}) direction=({},{}) - applying reflection",
                        getMaterialName(cell.material_type),
                        x,
                        y,
                        direction.x,
                        direction.y);

                    collision_calculator_.applyBoundaryReflection(cell, direction);
                    boundary_reflection_applied = true;
                }
            }

            // Always update the COM components that didn't cross boundaries.
            // This allows water to move horizontally even when hitting vertical boundaries.
            if (!boundary_reflection_applied) {
                // No reflections, update entire COM.
                cell.setCOM(newCOM);
            }
            else {
                // Reflections occurred. Update non-reflected components.
                Vector2d currentCOM = cell.com;
                Vector2d updatedCOM = currentCOM;

                // Check which boundaries were NOT crossed and update those components.
                bool x_reflected = false;
                bool y_reflected = false;

                for (const Vector2i& dir : crossed_boundaries) {
                    if (dir.x != 0) x_reflected = true;
                    if (dir.y != 0) y_reflected = true;
                }

                // Update components that didn't cross boundaries.
                if (!x_reflected && std::abs(newCOM.x) < 1.0) {
                    updatedCOM.x = newCOM.x;
                }
                if (!y_reflected && std::abs(newCOM.y) < 1.0) {
                    updatedCOM.y = newCOM.y;
                }

                cell.setCOM(updatedCOM);
            }
        }
    }

    return moves;
}

void World::processMaterialMoves()
{
    ScopeTimer timer(timers_, "process_moves");

    // Shuffle moves to handle conflicts randomly.
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::shuffle(pending_moves_.begin(), pending_moves_.end(), gen);

    for (const auto& move : pending_moves_) {
        Cell& fromCell = at(move.fromX, move.fromY);
        Cell& toCell = at(move.toX, move.toY);

        // Apply any pressure from excess that couldn't transfer.
        if (move.pressure_from_excess > 0.0) {
            // If target is a wall, pressure reflects back to source.
            if (toCell.material_type == MaterialType::WALL) {
                fromCell.setDynamicPressure(fromCell.dynamic_component + move.pressure_from_excess);

                spdlog::debug(
                    "Wall blocked transfer: source cell({},{}) pressure increased by {:.3f}",
                    move.fromX,
                    move.fromY,
                    move.pressure_from_excess);
            }
            else {
                // Normal materials receive the pressure.
                toCell.setDynamicPressure(toCell.dynamic_component + move.pressure_from_excess);

                spdlog::debug(
                    "Applied pressure from excess: cell({},{}) pressure increased by {:.3f}",
                    move.toX,
                    move.toY,
                    move.pressure_from_excess);
            }
        }

        // Handle collision during the move based on collision_type.
        if (move.collision_type != CollisionType::TRANSFER_ONLY) {
            spdlog::debug(
                "Processing collision: {} vs {} at ({},{}) -> ({},{}) - Type: {}",
                getMaterialName(move.material),
                getMaterialName(toCell.material_type),
                move.fromX,
                move.fromY,
                move.toX,
                move.toY,
                static_cast<int>(move.collision_type));
        }

        switch (move.collision_type) {
            case CollisionType::TRANSFER_ONLY:
                collision_calculator_.handleTransferMove(*this, fromCell, toCell, move);
                break;
            case CollisionType::ELASTIC_REFLECTION:
                collision_calculator_.handleElasticCollision(fromCell, toCell, move);
                break;
            case CollisionType::INELASTIC_COLLISION:
                collision_calculator_.handleInelasticCollision(*this, fromCell, toCell, move);
                break;
            case CollisionType::FRAGMENTATION:
                collision_calculator_.handleFragmentation(*this, fromCell, toCell, move);
                break;
            case CollisionType::ABSORPTION:
                collision_calculator_.handleAbsorption(*this, fromCell, toCell, move);
                break;
        }
    }

    pending_moves_.clear();
}

void World::setupBoundaryWalls()
{
    spdlog::info("Setting up boundary walls for World");

    // Top and bottom walls.
    for (uint32_t x = 0; x < data.width; ++x) {
        at(x, 0).replaceMaterial(MaterialType::WALL, 1.0);
        at(x, data.height - 1).replaceMaterial(MaterialType::WALL, 1.0);
    }

    // Left and right walls.
    for (uint32_t y = 0; y < data.height; ++y) {
        at(0, y).replaceMaterial(MaterialType::WALL, 1.0);
        at(data.width - 1, y).replaceMaterial(MaterialType::WALL, 1.0);
    }

    spdlog::info("Boundary walls setup complete");
}

// =================================================================.
// HELPER METHODS.
// =================================================================.

void World::pixelToCell(int pixelX, int pixelY, int& cellX, int& cellY) const
{
    // Convert pixel coordinates to cell coordinates.
    // Each cell is Cell::WIDTH x Cell::HEIGHT pixels.
    cellX = pixelX / Cell::WIDTH;
    cellY = pixelY / Cell::HEIGHT;
}

bool World::isValidCell(int x, int y) const
{
    return x >= 0 && y >= 0 && static_cast<uint32_t>(x) < data.width
        && static_cast<uint32_t>(y) < data.height;
}

size_t World::coordToIndex(uint32_t x, uint32_t y) const
{
    return y * data.width + x;
}

Vector2i World::pixelToCell(int pixelX, int pixelY) const
{
    return Vector2i(pixelX / Cell::WIDTH, pixelY / Cell::HEIGHT);
}

bool World::isValidCell(const Vector2i& pos) const
{
    return isValidCell(pos.x, pos.y);
}

size_t World::coordToIndex(const Vector2i& pos) const
{
    return coordToIndex(static_cast<uint32_t>(pos.x), static_cast<uint32_t>(pos.y));
}

// =================================================================.
// WORLD SETUP CONTROL METHODS.
// =================================================================.

void World::setWallsEnabled(bool enabled)
{
    ConfigurableWorldEventGenerator* configSetup =
        dynamic_cast<ConfigurableWorldEventGenerator*>(worldEventGenerator_.get());
    if (configSetup) {
        configSetup->setWallsEnabled(enabled);
    }

    // Rebuild walls if needed.
    if (enabled) {
        setupBoundaryWalls();
    }
    else {
        // Clear existing walls by resetting boundary cells to air.
        for (uint32_t x = 0; x < data.width; ++x) {
            at(x, 0).clear();               // Top wall.
            at(x, data.height - 1).clear(); // Bottom wall.
        }
        for (uint32_t y = 0; y < data.height; ++y) {
            at(0, y).clear();              // Left wall.
            at(data.width - 1, y).clear(); // Right wall.
        }
    }
}

bool World::areWallsEnabled() const
{
    const ConfigurableWorldEventGenerator* configSetup =
        dynamic_cast<const ConfigurableWorldEventGenerator*>(worldEventGenerator_.get());
    return configSetup ? configSetup->areWallsEnabled() : false;
}

void World::setHydrostaticPressureEnabled(bool enabled)
{
    // Backward compatibility: set strength to 0 (disabled) or default (enabled).
    physicsSettings.pressure_hydrostatic_strength = enabled ? 1.0 : 0.0;

    spdlog::info("Clearing all pressure values");
    for (auto& cell : data.cells) {
        cell.setHydrostaticPressure(0.0);
        if (cell.dynamic_component < MIN_MATTER_THRESHOLD) {
            cell.pressure_gradient = Vector2d{ 0.0, 0.0 };
        }
    }
}

void World::setDynamicPressureEnabled(bool enabled)
{
    // Backward compatibility: set strength to 0 (disabled) or default (enabled).
    physicsSettings.pressure_dynamic_strength = enabled ? 1.0 : 0.0;

    spdlog::info("Clearing all pressure values");
    for (auto& cell : data.cells) {
        cell.setDynamicPressure(0.0);
        if (cell.hydrostatic_component < MIN_MATTER_THRESHOLD) {
            cell.pressure_gradient = Vector2d{ 0.0, 0.0 };
        }
    }

    // Clear any pending blocked transfers.
    pressure_calculator_.blocked_transfers_.clear();
}

void World::setHydrostaticPressureStrength(double strength)
{
    physicsSettings.pressure_hydrostatic_strength = strength;
    spdlog::info("Hydrostatic pressure strength set to {:.2f}", strength);
}

double World::getHydrostaticPressureStrength() const
{
    return physicsSettings.pressure_hydrostatic_strength;
}

void World::setDynamicPressureStrength(double strength)
{
    physicsSettings.pressure_dynamic_strength = strength;
    spdlog::info("Dynamic pressure strength set to {:.2f}", strength);
}

double World::getDynamicPressureStrength() const
{
    return physicsSettings.pressure_dynamic_strength;
}

std::string World::settingsToString() const
{
    std::stringstream ss;
    ss << "=== World Settings ===\n";
    ss << "Grid size: " << data.width << "x" << data.height << "\n";
    ss << "Gravity: " << physicsSettings.gravity << "\n";
    ss << "Hydrostatic pressure enabled: " << (isHydrostaticPressureEnabled() ? "true" : "false")
       << "\n";
    ss << "Dynamic pressure enabled: " << (isDynamicPressureEnabled() ? "true" : "false") << "\n";
    ss << "Pressure scale: " << physicsSettings.pressure_scale << "\n";
    ss << "Elasticity factor: " << physicsSettings.elasticity << "\n";
    ss << "Add particles enabled: " << (data.add_particles_enabled ? "true" : "false") << "\n";
    ss << "Walls enabled: " << (areWallsEnabled() ? "true" : "false") << "\n";
    ss << "Rain rate: " << getRainRate() /* stub */ << "\n";
    ss << "Left throw enabled: " << (isLeftThrowEnabled() ? "true" : "false") << "\n";
    ss << "Right throw enabled: " << (isRightThrowEnabled() ? "true" : "false") << "\n";
    ss << "Lower right quadrant enabled: " << (isLowerRightQuadrantEnabled() ? "true" : "false")
       << "\n";
    ss << "Cohesion COM force enabled: " << (isCohesionComForceEnabled() ? "true" : "false")
       << "\n";
    ss << "Cohesion bind force enabled: " << (isCohesionBindForceEnabled() ? "true" : "false")
       << "\n";
    ss << "Adhesion enabled: " << (adhesion_calculator_.isAdhesionEnabled() ? "true" : "false")
       << "\n";
    ss << "Air resistance enabled: " << (air_resistance_enabled_ ? "true" : "false") << "\n";
    ss << "Air resistance strength: " << air_resistance_strength_ << "\n";
    ss << "Material removal threshold: " << MIN_MATTER_THRESHOLD << "\n";
    return ss.str();
}

// World type management implementation.
void World::setWorldEventGenerator(std::shared_ptr<WorldEventGenerator> newSetup)
{
    worldEventGenerator_ = std::move(newSetup);
    // Reset and apply the new setup.
    if (worldEventGenerator_) {
        this->setup();
    }
}

WorldEventGenerator* World::getWorldEventGenerator() const
{
    return worldEventGenerator_.get();
}

// =================================================================
// JSON SERIALIZATION
// =================================================================

nlohmann::json World::toJSON() const
{
    // Automatic serialization via ReflectSerializer!
    return ReflectSerializer::to_json(data);
}

void World::fromJSON(const nlohmann::json& doc)
{
    // Automatic deserialization via ReflectSerializer!
    data = ReflectSerializer::from_json<WorldData>(doc);
    spdlog::info("World deserialized: {}x{} grid", data.width, data.height);
}

// Stub implementations for WorldInterface methods.
void World::onPreResize(uint32_t newWidth, uint32_t newHeight)
{
    spdlog::debug(
        "World::onPreResize: {}x{} -> {}x{}", data.width, data.height, newWidth, newHeight);
}

bool World::shouldResize(uint32_t newWidth, uint32_t newHeight) const
{
    return data.width != newWidth || data.height != newHeight;
}

// ADL functions for MotionState JSON serialization.
void to_json(nlohmann::json& j, World::MotionState state)
{
    switch (state) {
        case World::MotionState::STATIC:
            j = "STATIC";
            break;
        case World::MotionState::FALLING:
            j = "FALLING";
            break;
        case World::MotionState::SLIDING:
            j = "SLIDING";
            break;
        case World::MotionState::TURBULENT:
            j = "TURBULENT";
            break;
    }
}

void from_json(const nlohmann::json& j, World::MotionState& state)
{
    std::string str = j.get<std::string>();
    if (str == "STATIC") {
        state = World::MotionState::STATIC;
    }
    else if (str == "FALLING") {
        state = World::MotionState::FALLING;
    }
    else if (str == "SLIDING") {
        state = World::MotionState::SLIDING;
    }
    else if (str == "TURBULENT") {
        state = World::MotionState::TURBULENT;
    }
    else {
        throw std::runtime_error("Unknown MotionState: " + str);
    }
}

void World::spawnMaterialBall(
    MaterialType material, uint32_t centerX, uint32_t centerY, uint32_t radius)
{
    // Spawn a ball of material centered at (centerX, centerY) with given radius.
    for (uint32_t y = 0; y < data.height; ++y) {
        for (uint32_t x = 0; x < data.width; ++x) {
            // Calculate distance from center.
            int dx = static_cast<int>(x) - static_cast<int>(centerX);
            int dy = static_cast<int>(y) - static_cast<int>(centerY);
            double distance = std::sqrt(dx * dx + dy * dy);

            // If within radius, fill the cell.
            if (distance <= static_cast<double>(radius)) {
                addMaterialAtCell(x, y, material, 1.0);
            }
        }
    }
}

} // namespace DirtSim
