#include "World.h"
#include "Cell.h"
#include "GridOfCells.h"
#include "ReflectSerializer.h"
#include "ScopeTimer.h"
#include "Vector2i.h"
#include "WorldAirResistanceCalculator.h"
#include "WorldCohesionCalculator.h"
#include "WorldCollisionCalculator.h"
#include "WorldDiagramGeneratorEmoji.h"
#include "WorldInterpolationTool.h"
#include "WorldSupportCalculator.h"
#include "WorldViscosityCalculator.h"
#include "organisms/TreeManager.h"
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

// Velocities are in Cells/second.
static constexpr double MAX_VELOCITY_PER_TIMESTEP = 40.0;
static constexpr double VELOCITY_DAMPING_THRESHOLD_PER_TIMESTEP = 20.0;
static constexpr double VELOCITY_DAMPING_FACTOR_PER_TIMESTEP = 0.10;

World::World() : World(1, 1)
{}

World::World(uint32_t width, uint32_t height)
    : cohesion_bind_force_enabled_(false),
      cohesion_bind_force_strength_(1.0),
      com_cohesion_range_(1),
      air_resistance_enabled_(true),
      air_resistance_strength_(0.1),
      selected_material_(MaterialType::DIRT),
      support_calculator_(),
      pressure_calculator_(),
      collision_calculator_(),
      adhesion_calculator_(),
      friction_calculator_(),
      tree_manager_(std::make_unique<TreeManager>()),
      rng_(std::make_unique<std::mt19937>(std::random_device{}()))
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

void World::setRandomSeed(uint32_t seed)
{
    rng_ = std::make_unique<std::mt19937>(seed);
    spdlog::debug("World RNG seed set to {}", seed);
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
    if (scaledDeltaTime == 0.0) {
        return;
    }

    // NOTE: Particle generation now handled by Scenario::tick(), called before advanceTime().

    // Build grid cache for optimized empty cell and material lookups.
    GridOfCells grid(data.cells, data.width, data.height, timers_);

    // Pre-compute support map for all cells (bottom-up pass).
    {
        ScopeTimer supportMapTimer(timers_, "compute_support_map");
        WorldSupportCalculator support_calc{ grid };
        support_calc.computeSupportMapBottomUp(*this);
    }

    // Calculate hydrostatic pressure based on current material positions.
    // This must happen before force resolution so buoyancy forces are immediate.
    if (physicsSettings.pressure_hydrostatic_strength > 0.0) {
        ScopeTimer hydroTimer(timers_, "hydrostatic_pressure");
        pressure_calculator_.calculateHydrostaticPressure(*this);
    }

    // Accumulate and apply all forces based on resistance.
    // This now includes pressure forces from the current frame.
    {
        ScopeTimer resolveTimer(timers_, "resolve_forces_total");
        resolveForces(scaledDeltaTime, &grid);
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

    // Process any blocked transfers that were queued during processMaterialMoves.
    // This generates dynamic pressure from collisions.
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

    // Update tree organisms after physics is complete.
    if (tree_manager_) {
        ScopeTimer treeTimer(timers_, "tree_organisms");
        tree_manager_->update(*this, scaledDeltaTime);
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

// DEPRECATED: World setup now handled by Scenario::setup().
void World::setup()
{
    spdlog::warn("World::setup() is deprecated - use Scenario::setup() instead");
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

    for (auto& cell : data.cells) {
        if (!cell.isEmpty() && !cell.isWall()) {
            // Gravity force is proportional to material density (F = m × g).
            // This enables buoyancy: denser materials sink, lighter materials float.
            const MaterialProperties& props = getMaterialProperties(cell.material_type);
            Vector2d gravityForce(0.0, props.density * physicsSettings.gravity);

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
    if (physicsSettings.cohesion_strength <= 0.0) {
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
                        * com_cohesion.force_magnitude * physicsSettings.cohesion_strength;

                    // Directional correction: only apply force when velocity is misaligned.
                    // This prevents oscillations by not accelerating already-converging particles.
                    if (cell.velocity.magnitude() > 0.01) {
                        double alignment = cell.velocity.dot(com_cohesion_force.normalize());

                        // Reduce force when already aligned with cohesion direction.
                        // alignment = -1.0 (opposite) → 100% force (strong correction)
                        // alignment =  0.0 (perpendicular) → 100% force (neutral)
                        // alignment = +1.0 (aligned) → 0% force (no interference)
                        double correction_factor = std::max(0.0, 1.0 - alignment);
                        com_cohesion_force = com_cohesion_force * correction_factor;
                    }

                    cell.addPendingForce(com_cohesion_force);
                }
            }
        }
    }

    // Adhesion force accumulation (only if enabled).
    if (physicsSettings.adhesion_strength > 0.0) {
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
                    * physicsSettings.adhesion_strength;
                cell.addPendingForce(adhesion_force);
            }
        }
    }
}

void World::applyPressureForces()
{
    if (physicsSettings.pressure_hydrostatic_strength <= 0.0
        && physicsSettings.pressure_dynamic_strength <= 0.0) {
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
                // Get material-specific hydrostatic weight to scale pressure response.
                const MaterialProperties& props = getMaterialProperties(cell.material_type);
                double hydrostatic_weight = props.hydrostatic_weight;

                Vector2d pressure_force =
                    gradient * physicsSettings.pressure_scale * hydrostatic_weight;
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

void World::resolveForces(double deltaTime, const GridOfCells* grid)
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

    // Apply viscous forces (momentum diffusion between same-material neighbors).
    if (physicsSettings.viscosity_strength > 0.0) {
        ScopeTimer viscosityTimer(timers_, "apply_viscous_forces");
        for (uint32_t y = 0; y < data.height; ++y) {
            for (uint32_t x = 0; x < data.width; ++x) {
                Cell& cell = at(x, y);

                if (cell.isEmpty() || cell.isWall()) {
                    continue;
                }

                // Calculate viscous force from neighbor velocity averaging.
                auto viscous_result = viscosity_calculator_.calculateViscousForce(*this, x, y, grid);
                cell.addPendingForce(viscous_result.force);

                // Store for visualization.
                cell.accumulated_viscous_force = viscous_result.force;
            }
        }
    }

    // Now resolve all accumulated forces directly (no damping).
    for (uint32_t y = 0; y < data.height; ++y) {
        for (uint32_t x = 0; x < data.width; ++x) {
            Cell& cell = at(x, y);

            if (cell.isEmpty() || cell.isWall()) {
                continue;
            }

            // Get the total pending force (includes gravity, pressure, cohesion, adhesion,
            // friction, viscosity).
            Vector2d net_force = cell.pending_force;

            // Apply forces directly to velocity (no damping factor!).
            Vector2d velocity_change = net_force * deltaTime;
            cell.velocity += velocity_change;

            // Debug logging.
            if (net_force.magnitude() > 0.001) {
                spdlog::debug(
                    "Cell ({},{}) {} - Force: ({:.3f},{:.3f}), vel_change: ({:.3f},{:.3f}), "
                    "new_vel: ({:.3f},{:.3f})",
                    x,
                    y,
                    getMaterialName(cell.material_type),
                    net_force.x,
                    net_force.y,
                    velocity_change.x,
                    velocity_change.y,
                    cell.velocity.x,
                    cell.velocity.y);
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
            if (physicsSettings.cohesion_strength > 0.0) {
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
                adhesion.force_magnitude * physicsSettings.adhesion_strength;
            double effective_com_cohesion_magnitude =
                com_cohesion.force_magnitude * physicsSettings.cohesion_strength;

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
    std::shuffle(pending_moves_.begin(), pending_moves_.end(), *rng_);

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

        // Check if materials should swap instead of colliding (if enabled).
        if (physicsSettings.swap_enabled) {
            Vector2i direction(move.toX - move.fromX, move.toY - move.fromY);
            if (collision_calculator_.shouldSwapMaterials(fromCell, toCell, direction, move)) {
                collision_calculator_.swapCounterMovingMaterials(fromCell, toCell, direction, move);
                continue; // Skip normal collision handling.
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

        // Track organism_id before transfer (in case source cell becomes empty).
        TreeId organism_id = fromCell.organism_id;

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

        // Record organism transfer if material had organism ownership.
        if (organism_id != 0 && move.collision_type == CollisionType::TRANSFER_ONLY) {
            // Transfer occurred - record it for TreeManager update.
            recordOrganismTransfer(
                move.fromX, move.fromY, move.toX, move.toY, organism_id, move.amount);
        }
    }

    pending_moves_.clear();

    // Notify TreeManager of all organism transfers for efficient tracking updates.
    if (!organism_transfers_.empty() && tree_manager_) {
        tree_manager_->notifyTransfers(organism_transfers_);
        organism_transfers_.clear();
    }
}

void World::recordOrganismTransfer(
    int fromX, int fromY, int toX, int toY, TreeId organism_id, double amount)
{
    organism_transfers_.push_back(
        OrganismTransfer{ Vector2i{ fromX, fromY }, Vector2i{ toX, toY }, organism_id, amount });
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

// DEPRECATED: Wall management now handled by scenarios.
void World::setWallsEnabled(bool enabled)
{
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
    // Check if boundary cells are walls.
    return at(0, 0).isWall();
}

// Legacy pressure toggle methods removed - use physicsSettings directly.

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
    ss << "Cohesion COM force enabled: "
       << (physicsSettings.cohesion_strength > 0.0 ? "true" : "false") << "\n";
    ss << "Cohesion bind force enabled: " << (isCohesionBindForceEnabled() ? "true" : "false")
       << "\n";
    ss << "Adhesion enabled: " << (physicsSettings.adhesion_strength > 0.0 ? "true" : "false")
       << "\n";
    ss << "Air resistance enabled: " << (air_resistance_enabled_ ? "true" : "false") << "\n";
    ss << "Air resistance strength: " << air_resistance_strength_ << "\n";
    ss << "Material removal threshold: " << MIN_MATTER_THRESHOLD << "\n";
    return ss.str();
}

// DEPRECATED: WorldEventGenerator removed - scenarios now handle setup/tick directly.

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

void World::spawnMaterialBall(MaterialType material, uint32_t centerX, uint32_t centerY)
{
    // Calculate radius as 15% of world width (diameter = 15% of width).
    double diameter = data.width * 0.15;
    double radius = diameter / 2.0;

    // Round up to ensure at least 1 cell for very small worlds.
    uint32_t radiusInt = static_cast<uint32_t>(std::ceil(radius));
    if (radiusInt < 1) {
        radiusInt = 1;
    }

    // Clamp center position to ensure ball fits within walls.
    // Walls occupy the outermost layer (x=0, x=width-1, y=0, y=height-1).
    // Valid interior range: [1, width-2] for x, [1, height-2] for y.
    uint32_t minX = 1 + radiusInt;
    uint32_t maxX = data.width >= 2 + radiusInt ? data.width - 1 - radiusInt : 1;
    uint32_t minY = 1 + radiusInt;
    uint32_t maxY = data.height >= 2 + radiusInt ? data.height - 1 - radiusInt : 1;

    // Clamp the provided center to valid range.
    uint32_t clampedCenterX = std::max(minX, std::min(centerX, maxX));
    uint32_t clampedCenterY = std::max(minY, std::min(centerY, maxY));

    // Only scan bounding box for efficiency.
    uint32_t scanMinX = clampedCenterX > radiusInt ? clampedCenterX - radiusInt : 0;
    uint32_t scanMaxX = std::min(clampedCenterX + radiusInt, data.width - 1);
    uint32_t scanMinY = clampedCenterY > radiusInt ? clampedCenterY - radiusInt : 0;
    uint32_t scanMaxY = std::min(clampedCenterY + radiusInt, data.height - 1);

    // Spawn a ball of material centered at the clamped position.
    for (uint32_t y = scanMinY; y <= scanMaxY; ++y) {
        for (uint32_t x = scanMinX; x <= scanMaxX; ++x) {
            // Calculate distance from center.
            int dx = static_cast<int>(x) - static_cast<int>(clampedCenterX);
            int dy = static_cast<int>(y) - static_cast<int>(clampedCenterY);
            double distance = std::sqrt(dx * dx + dy * dy);

            // If within radius, fill the cell.
            if (distance <= radius) {
                addMaterialAtCell(x, y, material, 1.0);
            }
        }
    }
}

} // namespace DirtSim
