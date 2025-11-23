#include "World.h"
#include "Cell.h"
#include "GridOfCells.h"
#include "PhysicsSettings.h"
#include "ReflectSerializer.h"
#include "ScopeTimer.h"
#include "Timers.h"
#include "Vector2i.h"
#include "WorldAdhesionCalculator.h"
#include "WorldAirResistanceCalculator.h"
#include "WorldCohesionCalculator.h"
#include "WorldCollisionCalculator.h"
#include "WorldData.h"
#include "WorldDiagramGeneratorEmoji.h"
#include "WorldFrictionCalculator.h"
#include "WorldInterpolationTool.h"
#include "WorldPressureCalculator.h"
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

// =================================================================
// PIMPL IMPLEMENTATION STRUCT
// =================================================================

struct World::Impl {
    // World state data (previously public).
    WorldData data_;

    // Physics settings (previously public).
    PhysicsSettings physicsSettings_;

    // Calculators (previously public).
    // WorldSupportCalculator removed - now constructed locally with GridOfCells reference.
    WorldPressureCalculator pressure_calculator_;
    WorldCollisionCalculator collision_calculator_;
    WorldAdhesionCalculator adhesion_calculator_;
    WorldFrictionCalculator friction_calculator_;
    WorldViscosityCalculator viscosity_calculator_;

    // Material transfer queue (internal simulation state).
    std::vector<MaterialMove> pending_moves_;

    // Organism transfer tracking (for efficient TreeManager updates).
    std::vector<OrganismTransfer> organism_transfers_;

    // Performance timing.
    mutable Timers timers_;

    // Constructor.
    Impl() { timers_.startTimer("total_simulation"); }

    // Destructor.
    ~Impl() { timers_.stopTimer("total_simulation"); }
};

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
      pImpl(),
      tree_manager_(std::make_unique<TreeManager>()),
      rng_(std::make_unique<std::mt19937>(std::random_device{}()))
{
    // Set dimensions (other WorldData members use defaults from struct declaration).
    pImpl->data_.width = width;
    pImpl->data_.height = height;

    spdlog::info(
        "Creating World: {}x{} grid with pure-material physics",
        pImpl->data_.width,
        pImpl->data_.height);

    // Initialize cell grid.
    pImpl->data_.cells.resize(pImpl->data_.width * pImpl->data_.height);

    // Initialize with air.
    for (auto& cell : pImpl->data_.cells) {
        cell = Cell{ MaterialType::AIR, 0.0 };
    }

    // Set up boundary walls if enabled.
    if (areWallsEnabled()) {
        setupBoundaryWalls();
    }

    spdlog::info("World initialization complete");
}

World::~World()
{
    spdlog::info("Destroying World: {}x{} grid", pImpl->data_.width, pImpl->data_.height);
}

// =================================================================
// CALCULATOR ACCESSORS
// =================================================================

WorldPressureCalculator& World::getPressureCalculator()
{
    return pImpl->pressure_calculator_;
}

const WorldPressureCalculator& World::getPressureCalculator() const
{
    return pImpl->pressure_calculator_;
}

WorldCollisionCalculator& World::getCollisionCalculator()
{
    return pImpl->collision_calculator_;
}

const WorldCollisionCalculator& World::getCollisionCalculator() const
{
    return pImpl->collision_calculator_;
}

WorldAdhesionCalculator& World::getAdhesionCalculator()
{
    return pImpl->adhesion_calculator_;
}

const WorldAdhesionCalculator& World::getAdhesionCalculator() const
{
    return pImpl->adhesion_calculator_;
}

WorldViscosityCalculator& World::getViscosityCalculator()
{
    return pImpl->viscosity_calculator_;
}

const WorldViscosityCalculator& World::getViscosityCalculator() const
{
    return pImpl->viscosity_calculator_;
}

WorldFrictionCalculator& World::getFrictionCalculator()
{
    return pImpl->friction_calculator_;
}

const WorldFrictionCalculator& World::getFrictionCalculator() const
{
    return pImpl->friction_calculator_;
}

Timers& World::getTimers()
{
    return pImpl->timers_;
}

const Timers& World::getTimers() const
{
    return pImpl->timers_;
}

void World::dumpTimerStats() const
{
    pImpl->timers_.dumpTimerStats();
}

WorldData& World::getData()
{
    return pImpl->data_;
}

const WorldData& World::getData() const
{
    return pImpl->data_;
}

PhysicsSettings& World::getPhysicsSettings()
{
    return pImpl->physicsSettings_;
}

const PhysicsSettings& World::getPhysicsSettings() const
{
    return pImpl->physicsSettings_;
}

// =================================================================
// SIMPLE GETTERS/SETTERS (moved from inline in header)
// =================================================================

void World::setSelectedMaterial(MaterialType type)
{
    selected_material_ = type;
}

MaterialType World::getSelectedMaterial() const
{
    return selected_material_;
}

void World::setDirtFragmentationFactor(double /* factor */)
{
    // No-op for World.
}

// =================================================================
// TIME REVERSAL STUBS (no-op implementations)
// =================================================================

void World::enableTimeReversal(bool /* enabled */)
{}
bool World::isTimeReversalEnabled() const
{
    return false;
}
void World::saveWorldState()
{}
bool World::canGoBackward() const
{
    return false;
}
bool World::canGoForward() const
{
    return false;
}
void World::goBackward()
{}
void World::goForward()
{}
void World::clearHistory()
{}
size_t World::getHistorySize() const
{
    return 0;
}

// =================================================================
// COHESION/ADHESION CONTROL
// =================================================================

void World::setCohesionBindForceEnabled(bool enabled)
{
    cohesion_bind_force_enabled_ = enabled;
}

bool World::isCohesionBindForceEnabled() const
{
    return cohesion_bind_force_enabled_;
}

void World::setCohesionComForceEnabled(bool enabled)
{
    pImpl->physicsSettings_.cohesion_enabled = enabled;
    pImpl->physicsSettings_.cohesion_strength = enabled ? 150.0 : 0.0;
}

bool World::isCohesionComForceEnabled() const
{
    return pImpl->physicsSettings_.cohesion_strength > 0.0;
}

void World::setCohesionComForceStrength(double strength)
{
    pImpl->physicsSettings_.cohesion_strength = strength;
}

double World::getCohesionComForceStrength() const
{
    return pImpl->physicsSettings_.cohesion_strength;
}

void World::setAdhesionStrength(double strength)
{
    pImpl->physicsSettings_.adhesion_strength = strength;
}

double World::getAdhesionStrength() const
{
    return pImpl->physicsSettings_.adhesion_strength;
}

void World::setAdhesionEnabled(bool enabled)
{
    pImpl->physicsSettings_.adhesion_enabled = enabled;
    pImpl->physicsSettings_.adhesion_strength = enabled ? 5.0 : 0.0;
}

bool World::isAdhesionEnabled() const
{
    return pImpl->physicsSettings_.adhesion_strength > 0.0;
}

void World::setCohesionBindForceStrength(double strength)
{
    cohesion_bind_force_strength_ = strength;
}

double World::getCohesionBindForceStrength() const
{
    return cohesion_bind_force_strength_;
}

// =================================================================
// VISCOSITY/FRICTION CONTROL
// =================================================================

void World::setViscosityStrength(double strength)
{
    pImpl->physicsSettings_.viscosity_strength = strength;
}

double World::getViscosityStrength() const
{
    return pImpl->physicsSettings_.viscosity_strength;
}

void World::setFrictionStrength(double strength)
{
    pImpl->physicsSettings_.friction_strength = strength;
}

double World::getFrictionStrength() const
{
    return pImpl->physicsSettings_.friction_strength;
}

void World::setCOMCohesionRange(uint32_t range)
{
    com_cohesion_range_ = range;
}

uint32_t World::getCOMCohesionRange() const
{
    return com_cohesion_range_;
}

// =================================================================
// AIR RESISTANCE CONTROL
// =================================================================

void World::setAirResistanceEnabled(bool enabled)
{
    air_resistance_enabled_ = enabled;
}

bool World::isAirResistanceEnabled() const
{
    return air_resistance_enabled_;
}

void World::setAirResistanceStrength(double strength)
{
    air_resistance_strength_ = strength;
}

double World::getAirResistanceStrength() const
{
    return air_resistance_strength_;
}

// =================================================================
// DEBUGGING/UTILITY
// =================================================================

void World::markUserInput()
{
    // No-op for now.
}

// =================================================================
// STUB METHODS (unimplemented features)
// =================================================================

void World::setRainRate(double /* rate */)
{}
double World::getRainRate() const
{
    return 0.0;
}
void World::setWaterColumnEnabled(bool /* enabled */)
{}
bool World::isWaterColumnEnabled() const
{
    return false;
}
void World::setLeftThrowEnabled(bool /* enabled */)
{}
bool World::isLeftThrowEnabled() const
{
    return false;
}
void World::setRightThrowEnabled(bool /* enabled */)
{}
bool World::isRightThrowEnabled() const
{
    return false;
}
void World::setLowerRightQuadrantEnabled(bool /* enabled */)
{}
bool World::isLowerRightQuadrantEnabled() const
{
    return false;
}

// =================================================================
// OTHER METHODS
// =================================================================

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
    ScopeTimer timer(pImpl->timers_, "advance_time");

    const double scaledDeltaTime = deltaTimeSeconds * pImpl->physicsSettings_.timescale;
    spdlog::debug(
        "World::advanceTime: deltaTime={:.4f}s, timestep={}",
        deltaTimeSeconds,
        pImpl->data_.timestep);
    if (scaledDeltaTime == 0.0) {
        return;
    }

    // Build grid cache for optimized empty cell and material lookups.
    GridOfCells grid(pImpl->data_.cells, pImpl->data_.width, pImpl->data_.height, pImpl->timers_);

    // Pre-compute support map for all cells (bottom-up pass).
    {
        ScopeTimer supportMapTimer(pImpl->timers_, "compute_support_map");
        WorldSupportCalculator support_calc{ grid };
        support_calc.computeSupportMapBottomUp(*this);
    }

    // Calculate hydrostatic pressure based on current material positions.
    // This must happen before force resolution so buoyancy forces are immediate.
    if (pImpl->physicsSettings_.pressure_hydrostatic_strength > 0.0) {
        ScopeTimer hydroTimer(pImpl->timers_, "hydrostatic_pressure");
        pImpl->pressure_calculator_.calculateHydrostaticPressure(*this);
    }

    // Accumulate and apply all forces based on resistance.
    // This now includes pressure forces from the current frame.
    resolveForces(scaledDeltaTime, grid);

    {
        ScopeTimer velocityTimer(pImpl->timers_, "velocity_limiting");
        processVelocityLimiting(scaledDeltaTime);
    }

    {
        ScopeTimer transfersTimer(pImpl->timers_, "update_transfers");
        updateTransfers(scaledDeltaTime);
    }

    // Process queued material moves - this detects NEW blocked transfers.
    processMaterialMoves();

    // Process any blocked transfers that were queued during processMaterialMoves.
    // This generates dynamic pressure from collisions.
    if (pImpl->physicsSettings_.pressure_dynamic_strength > 0.0) {
        ScopeTimer dynamicTimer(pImpl->timers_, "dynamic_pressure");
        // Generate virtual gravity transfers to create pressure from gravity forces.
        // This allows dynamic pressure to model hydrostatic-like behavior.
        //        pImpl->pressure_calculator_.generateVirtualGravityTransfers(scaledDeltaTime);

        pImpl->pressure_calculator_.processBlockedTransfers(
            *this, pImpl->pressure_calculator_.blocked_transfers_);
        pImpl->pressure_calculator_.blocked_transfers_.clear();
    }

    // Apply pressure diffusion before decay.
    if (pImpl->physicsSettings_.pressure_diffusion_strength > 0.0) {
        ScopeTimer diffusionTimer(pImpl->timers_, "pressure_diffusion");
        pImpl->pressure_calculator_.applyPressureDiffusion(*this, scaledDeltaTime);
    }

    // Apply pressure decay after material moves.
    {
        ScopeTimer decayTimer(pImpl->timers_, "pressure_decay");
        pImpl->pressure_calculator_.applyPressureDecay(*this, scaledDeltaTime);
    }

    // Update tree organisms after physics is complete.
    if (tree_manager_) {
        ScopeTimer treeTimer(pImpl->timers_, "tree_organisms");
        tree_manager_->update(*this, scaledDeltaTime);
    }

    pImpl->data_.timestep++;
}

void World::reset()
{
    spdlog::info("Resetting World to empty state");

    pImpl->data_.timestep = 0;
    pImpl->data_.removed_mass = 0.0;
    pImpl->pending_moves_.clear();

    // Clear all cells to air.
    for (auto& cell : pImpl->data_.cells) {
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

    Cell& cell = pImpl->data_.at(x, y);
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
        pImpl->data_.cells, pImpl->data_.width, pImpl->data_.height, newWidth, newHeight);

    // Phase 2: Update world state with the new interpolated cells.
    pImpl->data_.width = newWidth;
    pImpl->data_.height = newHeight;
    pImpl->data_.cells = std::move(interpolatedCells);

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

double World::getTotalMass() const
{
    double totalMass = 0.0;
    int cellCount = 0;
    int nonEmptyCells = 0;

    for (const auto& cell : pImpl->data_.cells) {
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
    // Cache pImpl members as local references.
    std::vector<Cell>& cells = pImpl->data_.cells;
    double gravity = pImpl->physicsSettings_.gravity;

    for (auto& cell : cells) {
        if (!cell.isEmpty() && !cell.isWall()) {
            // Gravity force is proportional to material density (F = m × g).
            // This enables buoyancy: denser materials sink, lighter materials float.
            const MaterialProperties& props = getMaterialProperties(cell.material_type);
            Vector2d gravityForce(0.0, props.density * gravity);

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

    // Cache pImpl members as local references.
    WorldData& data = pImpl->data_;

    WorldAirResistanceCalculator air_resistance_calculator{};

    for (uint32_t y = 0; y < data.height; ++y) {
        for (uint32_t x = 0; x < data.width; ++x) {
            Cell& cell = data.at(x, y);

            if (!cell.isEmpty() && !cell.isWall()) {
                Vector2d air_resistance_force = air_resistance_calculator.calculateAirResistance(
                    *this, x, y, air_resistance_strength_);
                cell.addPendingForce(air_resistance_force);
            }
        }
    }
}

void World::applyCohesionForces(const GridOfCells& grid)
{
    // Cache pImpl members as local references.
    PhysicsSettings& settings = pImpl->physicsSettings_;
    Timers& timers = pImpl->timers_;
    WorldData& data = pImpl->data_;
    WorldAdhesionCalculator& adhesion_calc = pImpl->adhesion_calculator_;

    if (settings.cohesion_strength <= 0.0) {
        return;
    }

    // Create calculators once outside the loop.
    WorldCohesionCalculator cohesion_calc{};

    {
        ScopeTimer cohesionTimer(timers, "cohesion_calculation");
        for (uint32_t y = 0; y < data.height; ++y) {
            for (uint32_t x = 0; x < data.width; ++x) {
                Cell& cell = data.at(x, y);

                if (cell.isEmpty() || cell.isWall()) {
                    continue;
                }

                // Calculate COM cohesion force (passes grid for cache optimization).
                WorldCohesionCalculator::COMCohesionForce com_cohesion =
                    cohesion_calc.calculateCOMCohesionForce(
                        *this, x, y, com_cohesion_range_, &grid);

                // Cache resistance for use in resolveForces (eliminates redundant calculation).
                const_cast<GridOfCells&>(grid).setCohesionResistance(
                    x, y, com_cohesion.resistance_magnitude);

                Vector2d com_cohesion_force(0.0, 0.0);
                if (com_cohesion.force_active) {
                    com_cohesion_force = com_cohesion.force_direction * com_cohesion.force_magnitude
                        * settings.cohesion_strength;

                    if (cell.velocity.magnitude() > 0.01) {
                        double alignment = cell.velocity.dot(com_cohesion_force.normalize());
                        double correction_factor = std::max(0.0, 1.0 - alignment);
                        com_cohesion_force = com_cohesion_force * correction_factor;
                    }

                    cell.addPendingForce(com_cohesion_force);
                }
                cell.accumulated_com_cohesion_force = com_cohesion_force;
            }
        }
    }

    // Adhesion force accumulation (only if enabled).
    if (settings.adhesion_strength > 0.0) {
        ScopeTimer adhesionTimer(timers, "adhesion_calculation");
        for (uint32_t y = 0; y < data.height; ++y) {
            for (uint32_t x = 0; x < data.width; ++x) {
                Cell& cell = data.at(x, y);

                if (cell.isEmpty() || cell.isWall()) {
                    continue;
                }

                // Use cache-optimized version with MaterialNeighborhood.
                const MaterialNeighborhood mat_n = grid.getMaterialNeighborhood(x, y);
                WorldAdhesionCalculator::AdhesionForce adhesion =
                    adhesion_calc.calculateAdhesionForce(*this, x, y, mat_n);
                Vector2d adhesion_force = adhesion.force_direction * adhesion.force_magnitude
                    * settings.adhesion_strength;
                cell.addPendingForce(adhesion_force);
                cell.accumulated_adhesion_force = adhesion_force;
            }
        }
    }
}

void World::applyPressureForces()
{
    // Cache pImpl members as local references.
    PhysicsSettings& settings = pImpl->physicsSettings_;
    WorldData& data = pImpl->data_;
    WorldPressureCalculator& pressure_calc = pImpl->pressure_calculator_;

    if (settings.pressure_hydrostatic_strength <= 0.0
        && settings.pressure_dynamic_strength <= 0.0) {
        return;
    }

    // Apply pressure forces through the pending force system.
    for (uint32_t y = 0; y < data.height; ++y) {
        for (uint32_t x = 0; x < data.width; ++x) {
            Cell& cell = data.at(x, y);

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
            Vector2d gradient = pressure_calc.calculatePressureGradient(*this, x, y);

            // Only apply force if system is out of equilibrium.
            if (gradient.magnitude() > 0.001) {
                // Get material-specific hydrostatic weight to scale pressure response.
                const MaterialProperties& props = getMaterialProperties(cell.material_type);
                double hydrostatic_weight = props.hydrostatic_weight;

                Vector2d pressure_force = gradient * settings.pressure_scale * hydrostatic_weight;
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

void World::resolveForces(double deltaTime, const GridOfCells& grid)
{
    // Cache frequently accessed pImpl members as local references to eliminate indirection
    // overhead.
    Timers& timers = pImpl->timers_;
    WorldViscosityCalculator& viscosity_calc = pImpl->viscosity_calculator_;
    PhysicsSettings& settings = pImpl->physicsSettings_;
    WorldData& data = pImpl->data_;
    std::vector<Cell>& cells = data.cells;

    ScopeTimer timer(timers, "resolve_forces");

    // Clear pending forces at the start of each physics frame.
    {
        ScopeTimer clearTimer(timers, "resolve_forces_clear_pending");
        for (auto& cell : cells) {
            cell.clearPendingForce();
        }
    }

    // Apply gravity forces.
    {
        ScopeTimer gravityTimer(timers, "resolve_forces_apply_gravity");
        applyGravity();
    }

    // Apply air resistance forces.
    {
        ScopeTimer airResistanceTimer(timers, "resolve_forces_apply_air_resistance");
        applyAirResistance();
    }

    // Apply pressure forces from previous frame.
    {
        ScopeTimer pressureTimer(timers, "resolve_forces_apply_pressure");
        applyPressureForces();
    }

    // Apply cohesion and adhesion forces.
    {
        ScopeTimer cohesionTimer(timers, "resolve_forces_apply_cohesion");
        applyCohesionForces(grid);
    }

    // Apply contact-based friction forces.
    {
        ScopeTimer frictionTimer(timers, "resolve_forces_apply_friction");
        pImpl->friction_calculator_.calculateAndApplyFrictionForces(*this, deltaTime);
    }

    // Apply viscous forces (momentum diffusion between same-material neighbors).
    if (settings.viscosity_strength > 0.0) {
        ScopeTimer viscosityTimer(timers, "apply_viscous_forces");
        double visc_strength = settings.viscosity_strength; // Cache once for entire loop.
        for (uint32_t y = 0; y < data.height; ++y) {
            for (uint32_t x = 0; x < data.width; ++x) {
                Cell& cell = data.at(x, y);

                if (cell.isEmpty() || cell.isWall()) {
                    continue;
                }

                // Calculate viscous force from neighbor velocity averaging.
                auto viscous_result =
                    viscosity_calc.calculateViscousForce(*this, x, y, visc_strength, &grid);
                cell.addPendingForce(viscous_result.force);

                // Store for visualization.
                cell.accumulated_viscous_force = viscous_result.force;
            }
        }
    }

    // Now resolve all accumulated forces directly (no damping).
    {
        ScopeTimer resolutionLoopTimer(timers, "resolve_forces_resolution_loop");
        for (uint32_t y = 0; y < data.height; ++y) {
            for (uint32_t x = 0; x < data.width; ++x) {
                Cell& cell = data.at(x, y);

                if (cell.isEmpty() || cell.isWall()) {
                    continue;
                }

                // Get the total pending force (includes gravity, pressure, cohesion, adhesion,
                // friction, viscosity).
                Vector2d net_force = cell.pending_force;

                // Check cohesion resistance threshold.
                double net_force_magnitude = net_force.magnitude();

                // Use cached cohesion strength (computed during applyCohesionForces).
                double cohesion_strength = grid.getCohesionResistance(x, y);
                double cohesion_resistance_force =
                    cohesion_strength * settings.cohesion_resistance_factor;

                if (cohesion_resistance_force > 0.01
                    && net_force_magnitude < cohesion_resistance_force) {
                    spdlog::debug(
                        "Force blocked: {} at ({},{}) held by cohesion (force: {:.3f} < "
                        "resistance: "
                        "{:.3f})",
                        getMaterialName(cell.material_type),
                        x,
                        y,
                        net_force_magnitude,
                        cohesion_resistance_force);
                    continue;
                }

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
}

void World::processVelocityLimiting(double deltaTime)
{
    for (auto& cell : pImpl->data_.cells) {
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
    ScopeTimer timer(pImpl->timers_, "update_transfers");

    // Clear previous moves.
    pImpl->pending_moves_.clear();

    // Compute material moves based on COM positions and velocities.
    pImpl->pending_moves_ = computeMaterialMoves(deltaTime);
}

std::vector<MaterialMove> World::computeMaterialMoves(double deltaTime)
{
    // Cache pImpl members as local references.
    WorldCollisionCalculator& collision_calc = pImpl->collision_calculator_;
    WorldData& data = pImpl->data_;

    // Pre-allocate moves vector based on previous frame's count.
    static size_t last_move_count = 0;
    std::vector<MaterialMove> moves;
    moves.reserve(last_move_count + last_move_count / 10); // +10% buffer

    // Counters for move generation analysis.
    size_t num_cells_with_velocity = 0;
    size_t num_boundary_crossings = 0;
    size_t num_moves_generated = 0;
    size_t num_transfers_generated = 0;
    size_t num_collisions_generated = 0;

    for (uint32_t y = 0; y < data.height; ++y) {
        for (uint32_t x = 0; x < data.width; ++x) {
            Cell& cell = data.at(x, y);

            if (cell.isEmpty() || cell.isWall()) {
                continue;
            }

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
            BoundaryCrossings crossed_boundaries = collision_calc.getAllBoundaryCrossings(newCOM);

            if (!crossed_boundaries.empty()) {
                num_cells_with_velocity++;
                num_boundary_crossings += crossed_boundaries.count;

                spdlog::debug(
                    "Boundary crossings detected for {} at ({},{}) with COM ({:.2f},{:.2f}) -> {} "
                    "crossings",
                    getMaterialName(cell.material_type),
                    x,
                    y,
                    newCOM.x,
                    newCOM.y,
                    crossed_boundaries.count);
            }

            bool boundary_reflection_applied = false;

            for (uint8_t i = 0; i < crossed_boundaries.count; ++i) {
                const Vector2i& direction = crossed_boundaries.dirs[i];
                Vector2i targetPos = Vector2i(x, y) + direction;

                if (isValidCell(targetPos)) {
                    // Create enhanced MaterialMove with collision physics data.
                    MaterialMove move = collision_calc.createCollisionAwareMove(
                        *this,
                        cell,
                        data.at(targetPos.x, targetPos.y),
                        Vector2i(x, y),
                        targetPos,
                        direction,
                        deltaTime);

                    num_moves_generated++;
                    if (move.collision_type == CollisionType::TRANSFER_ONLY) {
                        num_transfers_generated++;
                    }
                    else {
                        num_collisions_generated++;
                    }

                    // Debug logging for collision detection.
                    if (move.collision_type != CollisionType::TRANSFER_ONLY) {
                        spdlog::debug(
                            "Collision detected: {} vs {} at ({},{}) -> ({},{}) - Type: {}, "
                            "Energy: {:.3f}",
                            getMaterialName(move.material),
                            getMaterialName(data.at(targetPos.x, targetPos.y).material_type),
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

                    collision_calc.applyBoundaryReflection(cell, direction);
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

                for (uint8_t i = 0; i < crossed_boundaries.count; ++i) {
                    const Vector2i& dir = crossed_boundaries.dirs[i];
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

    // Log move generation statistics.
    spdlog::info(
        "computeMaterialMoves: {} cells moving, {} boundary crossings, {} moves generated ({} "
        "transfers, {} collisions)",
        num_cells_with_velocity,
        num_boundary_crossings,
        num_moves_generated,
        num_transfers_generated,
        num_collisions_generated);

    // Update last move count for next frame's pre-allocation.
    last_move_count = moves.size();

    return moves;
}

void World::processMaterialMoves()
{
    // Cache pImpl members as local references.
    Timers& timers = pImpl->timers_;
    WorldCollisionCalculator& collision_calc = pImpl->collision_calculator_;
    PhysicsSettings& settings = pImpl->physicsSettings_;
    WorldData& data = pImpl->data_;
    std::vector<MaterialMove>& pending_moves = pImpl->pending_moves_;
    std::vector<OrganismTransfer>& organism_transfers = pImpl->organism_transfers_;

    ScopeTimer timer(timers, "process_moves");

    // Counters for analysis.
    size_t num_moves = pending_moves.size();
    size_t num_swaps = 0;
    size_t num_swaps_from_transfers = 0;
    size_t num_swaps_from_collisions = 0;
    size_t num_transfers = 0;
    size_t num_elastic = 0;
    size_t num_inelastic = 0;

    // Shuffle moves to handle conflicts randomly.
    {
        ScopeTimer shuffleTimer(timers, "process_moves_shuffle");
        std::shuffle(pending_moves.begin(), pending_moves.end(), *rng_);
    }

    for (const auto& move : pending_moves) {
        Cell& fromCell = data.at(move.fromX, move.fromY);
        Cell& toCell = data.at(move.toX, move.toY);

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
        if (settings.swap_enabled) {
            Vector2i direction(move.toX - move.fromX, move.toY - move.fromY);
            bool should_swap = collision_calc.shouldSwapMaterials(
                *this, move.fromX, move.fromY, fromCell, toCell, direction, move);

            if (should_swap) {
                num_swaps++;
                if (move.collision_type == CollisionType::TRANSFER_ONLY) {
                    num_swaps_from_transfers++;
                }
                else {
                    num_swaps_from_collisions++;
                }
                TreeId from_organism = fromCell.organism_id;
                TreeId to_organism = toCell.organism_id;

                collision_calc.swapCounterMovingMaterials(fromCell, toCell, direction, move);

                if (from_organism != INVALID_TREE_ID) {
                    organism_transfers.push_back(
                        { Vector2i{ static_cast<int>(move.fromX), static_cast<int>(move.fromY) },
                          Vector2i{ static_cast<int>(move.toX), static_cast<int>(move.toY) },
                          from_organism,
                          fromCell.fill_ratio });
                }

                if (to_organism != INVALID_TREE_ID) {
                    organism_transfers.push_back(
                        { Vector2i{ static_cast<int>(move.toX), static_cast<int>(move.toY) },
                          Vector2i{ static_cast<int>(move.fromX), static_cast<int>(move.fromY) },
                          to_organism,
                          toCell.fill_ratio });
                }

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
                num_transfers++;
                collision_calc.handleTransferMove(*this, fromCell, toCell, move);
                break;
            case CollisionType::ELASTIC_REFLECTION:
                num_elastic++;
                collision_calc.handleElasticCollision(fromCell, toCell, move);
                break;
            case CollisionType::INELASTIC_COLLISION:
                num_inelastic++;
                collision_calc.handleInelasticCollision(*this, fromCell, toCell, move);
                break;
            case CollisionType::FRAGMENTATION:
                collision_calc.handleFragmentation(*this, fromCell, toCell, move);
                break;
            case CollisionType::ABSORPTION:
                collision_calc.handleAbsorption(*this, fromCell, toCell, move);
                break;
        }

        // Record organism transfer if material had organism ownership.
        if (organism_id != 0 && move.collision_type == CollisionType::TRANSFER_ONLY) {
            // Transfer occurred - record it for TreeManager update.
            recordOrganismTransfer(
                move.fromX, move.fromY, move.toX, move.toY, organism_id, move.amount);
        }
    }

    // Log move statistics.
    spdlog::info(
        "processMaterialMoves: {} total moves, {} swaps ({:.1f}% - {} from transfers, {} from "
        "collisions), {} transfers, {} elastic, {} inelastic",
        num_moves,
        num_swaps,
        num_moves > 0 ? (100.0 * num_swaps / num_moves) : 0.0,
        num_swaps_from_transfers,
        num_swaps_from_collisions,
        num_transfers,
        num_elastic,
        num_inelastic);

    pImpl->pending_moves_.clear();

    // Notify TreeManager of all organism transfers for efficient tracking updates.
    if (!organism_transfers.empty() && tree_manager_) {
        tree_manager_->notifyTransfers(organism_transfers);
        organism_transfers.clear();
    }
}

void World::recordOrganismTransfer(
    int fromX, int fromY, int toX, int toY, TreeId organism_id, double amount)
{
    pImpl->organism_transfers_.push_back(
        OrganismTransfer{ Vector2i{ fromX, fromY }, Vector2i{ toX, toY }, organism_id, amount });
}

void World::setupBoundaryWalls()
{
    spdlog::info("Setting up boundary walls for World");

    // Top and bottom walls.
    for (uint32_t x = 0; x < pImpl->data_.width; ++x) {
        pImpl->data_.at(x, 0).replaceMaterial(MaterialType::WALL, 1.0);
        pImpl->data_.at(x, pImpl->data_.height - 1).replaceMaterial(MaterialType::WALL, 1.0);
    }

    // Left and right walls.
    for (uint32_t y = 0; y < pImpl->data_.height; ++y) {
        pImpl->data_.at(0, y).replaceMaterial(MaterialType::WALL, 1.0);
        pImpl->data_.at(pImpl->data_.width - 1, y).replaceMaterial(MaterialType::WALL, 1.0);
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
    return x >= 0 && y >= 0 && static_cast<uint32_t>(x) < pImpl->data_.width
        && static_cast<uint32_t>(y) < pImpl->data_.height;
}

Vector2i World::pixelToCell(int pixelX, int pixelY) const
{
    return Vector2i(pixelX / Cell::WIDTH, pixelY / Cell::HEIGHT);
}

bool World::isValidCell(const Vector2i& pos) const
{
    return isValidCell(pos.x, pos.y);
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
        for (uint32_t x = 0; x < pImpl->data_.width; ++x) {
            pImpl->data_.at(x, 0).clear();                       // Top wall.
            pImpl->data_.at(x, pImpl->data_.height - 1).clear(); // Bottom wall.
        }
        for (uint32_t y = 0; y < pImpl->data_.height; ++y) {
            pImpl->data_.at(0, y).clear();                      // Left wall.
            pImpl->data_.at(pImpl->data_.width - 1, y).clear(); // Right wall.
        }
    }
}

bool World::areWallsEnabled() const
{
    // Check if boundary cells are walls.
    return pImpl->data_.at(0, 0).isWall();
}

// Legacy pressure toggle methods removed - use physicsSettings directly.

std::string World::settingsToString() const
{
    std::stringstream ss;
    ss << "=== World Settings ===\n";
    ss << "Grid size: " << pImpl->data_.width << "x" << pImpl->data_.height << "\n";
    ss << "Gravity: " << pImpl->physicsSettings_.gravity << "\n";
    ss << "Hydrostatic pressure enabled: "
       << (getPhysicsSettings().pressure_hydrostatic_strength > 0 ? "true" : "false") << "\n";
    ss << "Dynamic pressure enabled: "
       << (getPhysicsSettings().pressure_dynamic_strength > 0 ? "true" : "false") << "\n";
    ss << "Pressure scale: " << pImpl->physicsSettings_.pressure_scale << "\n";
    ss << "Elasticity factor: " << pImpl->physicsSettings_.elasticity << "\n";
    ss << "Add particles enabled: " << (pImpl->data_.add_particles_enabled ? "true" : "false")
       << "\n";
    ss << "Walls enabled: " << (areWallsEnabled() ? "true" : "false") << "\n";
    ss << "Rain rate: " << getRainRate() /* stub */ << "\n";
    ss << "Left throw enabled: " << (isLeftThrowEnabled() ? "true" : "false") << "\n";
    ss << "Right throw enabled: " << (isRightThrowEnabled() ? "true" : "false") << "\n";
    ss << "Lower right quadrant enabled: " << (isLowerRightQuadrantEnabled() ? "true" : "false")
       << "\n";
    ss << "Cohesion COM force enabled: "
       << (pImpl->physicsSettings_.cohesion_strength > 0.0 ? "true" : "false") << "\n";
    ss << "Cohesion bind force enabled: " << (isCohesionBindForceEnabled() ? "true" : "false")
       << "\n";
    ss << "Adhesion enabled: "
       << (pImpl->physicsSettings_.adhesion_strength > 0.0 ? "true" : "false") << "\n";
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
    return ReflectSerializer::to_json(pImpl->data_);
}

void World::fromJSON(const nlohmann::json& doc)
{
    // Automatic deserialization via ReflectSerializer!
    pImpl->data_ = ReflectSerializer::from_json<WorldData>(doc);
    spdlog::info("World deserialized: {}x{} grid", pImpl->data_.width, pImpl->data_.height);
}

// Stub implementations for WorldInterface methods.
void World::onPreResize(uint32_t newWidth, uint32_t newHeight)
{
    spdlog::debug(
        "World::onPreResize: {}x{} -> {}x{}",
        pImpl->data_.width,
        pImpl->data_.height,
        newWidth,
        newHeight);
}

bool World::shouldResize(uint32_t newWidth, uint32_t newHeight) const
{
    return pImpl->data_.width != newWidth || pImpl->data_.height != newHeight;
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
    double diameter = pImpl->data_.width * 0.15;
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
    uint32_t maxX = pImpl->data_.width >= 2 + radiusInt ? pImpl->data_.width - 1 - radiusInt : 1;
    uint32_t minY = 1 + radiusInt;
    uint32_t maxY = pImpl->data_.height >= 2 + radiusInt ? pImpl->data_.height - 1 - radiusInt : 1;

    // Clamp the provided center to valid range.
    uint32_t clampedCenterX = std::max(minX, std::min(centerX, maxX));
    uint32_t clampedCenterY = std::max(minY, std::min(centerY, maxY));

    // Only scan bounding box for efficiency.
    uint32_t scanMinX = clampedCenterX > radiusInt ? clampedCenterX - radiusInt : 0;
    uint32_t scanMaxX = std::min(clampedCenterX + radiusInt, pImpl->data_.width - 1);
    uint32_t scanMinY = clampedCenterY > radiusInt ? clampedCenterY - radiusInt : 0;
    uint32_t scanMaxY = std::min(clampedCenterY + radiusInt, pImpl->data_.height - 1);

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
