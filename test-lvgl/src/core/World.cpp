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
    WorldSupportCalculator support_calculator_;
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

WorldSupportCalculator& World::getSupportCalculator()
{
    return pImpl->support_calculator_;
}

const WorldSupportCalculator& World::getSupportCalculator() const
{
    return pImpl->support_calculator_;
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

Vector2d World::getGravityVector() const
{
    return Vector2d{ 0.0, pImpl->physicsSettings_.gravity };
}

void World::setDirtFragmentationFactor(double /* factor */)
{
    // No-op for World.
}

bool World::isHydrostaticPressureEnabled() const
{
    return pImpl->physicsSettings_.pressure_hydrostatic_strength > 0.0;
}

bool World::isDynamicPressureEnabled() const
{
    return pImpl->physicsSettings_.pressure_dynamic_strength > 0.0;
}

bool World::isPressureDiffusionEnabled() const
{
    return pImpl->physicsSettings_.pressure_diffusion_strength > 0.0;
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
    spdlog::trace(
        "World::advanceTime: deltaTime={:.4f}s, timestep={}",
        deltaTimeSeconds,
        pImpl->data_.timestep);
    if (scaledDeltaTime == 0.0) {
        return;
    }

    // NOTE: Particle generation now handled by Scenario::tick(), called before advanceTime().

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
    {
        ScopeTimer resolveTimer(pImpl->timers_, "resolve_forces_total");
        resolveForces(scaledDeltaTime, &grid);
    }

    {
        ScopeTimer velocityTimer(pImpl->timers_, "velocity_limiting");
        processVelocityLimiting(scaledDeltaTime);
    }

    {
        ScopeTimer transfersTimer(pImpl->timers_, "update_transfers");
        updateTransfers(scaledDeltaTime);
    }

    // Process queued material moves - this detects NEW blocked transfers.
    {
        ScopeTimer movesTimer(pImpl->timers_, "process_moves_total");
        processMaterialMoves();
    }

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

Cell& World::at(uint32_t x, uint32_t y)
{
    assert(x < pImpl->data_.width && y < pImpl->data_.height);
    return pImpl->data_.cells[coordToIndex(x, y)];
}

const Cell& World::at(uint32_t x, uint32_t y) const
{
    assert(x < pImpl->data_.width && y < pImpl->data_.height);
    return pImpl->data_.cells[coordToIndex(x, y)];
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
    ScopeTimer timer(pImpl->timers_, "apply_gravity");

    for (auto& cell : pImpl->data_.cells) {
        if (!cell.isEmpty() && !cell.isWall()) {
            // Gravity force is proportional to material density (F = m × g).
            // This enables buoyancy: denser materials sink, lighter materials float.
            const MaterialProperties& props = getMaterialProperties(cell.material_type);
            Vector2d gravityForce(0.0, props.density * pImpl->physicsSettings_.gravity);

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

    ScopeTimer timer(pImpl->timers_, "apply_air_resistance");

    WorldAirResistanceCalculator air_resistance_calculator{};

    for (uint32_t y = 0; y < pImpl->data_.height; ++y) {
        for (uint32_t x = 0; x < pImpl->data_.width; ++x) {
            Cell& cell = at(x, y);

            if (!cell.isEmpty() && !cell.isWall()) {
                Vector2d air_resistance_force = air_resistance_calculator.calculateAirResistance(
                    *this, x, y, air_resistance_strength_);
                cell.addPendingForce(air_resistance_force);
            }
        }
    }
}

void World::applyCohesionForces(const GridOfCells* grid)
{
    if (pImpl->physicsSettings_.cohesion_strength <= 0.0) {
        return;
    }

    ScopeTimer timer(pImpl->timers_, "apply_cohesion_forces");

    // Create calculators once outside the loop.
    WorldCohesionCalculator cohesion_calc{};

    {
        ScopeTimer cohesionTimer(pImpl->timers_, "cohesion_calculation");
        for (uint32_t y = 0; y < pImpl->data_.height; ++y) {
            for (uint32_t x = 0; x < pImpl->data_.width; ++x) {
                Cell& cell = at(x, y);

                if (cell.isEmpty() || cell.isWall()) {
                    continue;
                }

                // Calculate COM cohesion force (passes grid for cache optimization).
                WorldCohesionCalculator::COMCohesionForce com_cohesion =
                    cohesion_calc.calculateCOMCohesionForce(*this, x, y, com_cohesion_range_, grid);

                Vector2d com_cohesion_force(0.0, 0.0);
                if (com_cohesion.force_active) {
                    com_cohesion_force = com_cohesion.force_direction * com_cohesion.force_magnitude
                        * pImpl->physicsSettings_.cohesion_strength;

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
    if (pImpl->physicsSettings_.adhesion_strength > 0.0) {
        ScopeTimer adhesionTimer(pImpl->timers_, "adhesion_calculation");
        for (uint32_t y = 0; y < pImpl->data_.height; ++y) {
            for (uint32_t x = 0; x < pImpl->data_.width; ++x) {
                Cell& cell = at(x, y);

                if (cell.isEmpty() || cell.isWall()) {
                    continue;
                }

                WorldAdhesionCalculator::AdhesionForce adhesion =
                    pImpl->adhesion_calculator_.calculateAdhesionForce(*this, x, y);
                Vector2d adhesion_force = adhesion.force_direction * adhesion.force_magnitude
                    * pImpl->physicsSettings_.adhesion_strength;
                cell.addPendingForce(adhesion_force);
                cell.accumulated_adhesion_force = adhesion_force;
            }
        }
    }
}

void World::applyPressureForces()
{
    if (pImpl->physicsSettings_.pressure_hydrostatic_strength <= 0.0
        && pImpl->physicsSettings_.pressure_dynamic_strength <= 0.0) {
        return;
    }

    ScopeTimer timer(pImpl->timers_, "apply_pressure_forces");

    // Apply pressure forces through the pending force system.
    for (uint32_t y = 0; y < pImpl->data_.height; ++y) {
        for (uint32_t x = 0; x < pImpl->data_.width; ++x) {
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
            Vector2d gradient = pImpl->pressure_calculator_.calculatePressureGradient(*this, x, y);

            // Only apply force if system is out of equilibrium.
            if (gradient.magnitude() > 0.001) {
                // Get material-specific hydrostatic weight to scale pressure response.
                const MaterialProperties& props = getMaterialProperties(cell.material_type);
                double hydrostatic_weight = props.hydrostatic_weight;

                Vector2d pressure_force =
                    gradient * pImpl->physicsSettings_.pressure_scale * hydrostatic_weight;
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
    ScopeTimer timer(pImpl->timers_, "resolve_forces");

    // Clear pending forces at the start of each physics frame.
    {
        ScopeTimer clearTimer(pImpl->timers_, "resolve_forces_clear_pending");
        for (auto& cell : pImpl->data_.cells) {
            cell.clearPendingForce();
        }
    }

    // Apply gravity forces.
    {
        ScopeTimer gravityTimer(pImpl->timers_, "resolve_forces_apply_gravity");
        applyGravity();
    }

    // Apply air resistance forces.
    {
        ScopeTimer airResistanceTimer(pImpl->timers_, "resolve_forces_apply_air_resistance");
        applyAirResistance();
    }

    // Apply pressure forces from previous frame.
    {
        ScopeTimer pressureTimer(pImpl->timers_, "resolve_forces_apply_pressure");
        applyPressureForces();
    }

    // Apply cohesion and adhesion forces.
    {
        ScopeTimer cohesionTimer(pImpl->timers_, "resolve_forces_apply_cohesion");
        applyCohesionForces(grid);
    }

    // Apply contact-based friction forces.
    {
        ScopeTimer frictionTimer(pImpl->timers_, "resolve_forces_apply_friction");
        pImpl->friction_calculator_.calculateAndApplyFrictionForces(*this, deltaTime);
    }

    // Apply viscous forces (momentum diffusion between same-material neighbors).
    if (pImpl->physicsSettings_.viscosity_strength > 0.0) {
        ScopeTimer viscosityTimer(pImpl->timers_, "apply_viscous_forces");
        for (uint32_t y = 0; y < pImpl->data_.height; ++y) {
            for (uint32_t x = 0; x < pImpl->data_.width; ++x) {
                Cell& cell = at(x, y);

                if (cell.isEmpty() || cell.isWall()) {
                    continue;
                }

                // Calculate viscous force from neighbor velocity averaging.
                auto viscous_result =
                    pImpl->viscosity_calculator_.calculateViscousForce(*this, x, y, grid);
                cell.addPendingForce(viscous_result.force);

                // Store for visualization.
                cell.accumulated_viscous_force = viscous_result.force;
            }
        }
    }

    // Now resolve all accumulated forces directly (no damping).
    {
        ScopeTimer resolutionLoopTimer(pImpl->timers_, "resolve_forces_resolution_loop");
        for (uint32_t y = 0; y < pImpl->data_.height; ++y) {
            for (uint32_t x = 0; x < pImpl->data_.width; ++x) {
                Cell& cell = at(x, y);

                if (cell.isEmpty() || cell.isWall()) {
                    continue;
                }

                // Get the total pending force (includes gravity, pressure, cohesion, adhesion,
                // friction, viscosity).
                Vector2d net_force = cell.pending_force;

                // Check cohesion resistance threshold.
                double net_force_magnitude = net_force.magnitude();
                double cohesion_strength;
                {
                    ScopeTimer cohesionStrengthTimer(
                        pImpl->timers_, "resolve_forces_cohesion_strength_calc");
                    cohesion_strength =
                        pImpl->collision_calculator_.calculateCohesionStrength(cell, *this, x, y);
                }
                double cohesion_resistance_force =
                    cohesion_strength * pImpl->physicsSettings_.cohesion_resistance_factor;

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
    std::vector<MaterialMove> moves;

    for (uint32_t y = 0; y < pImpl->data_.height; ++y) {
        for (uint32_t x = 0; x < pImpl->data_.width; ++x) {
            Cell& cell = at(x, y);

            if (cell.isEmpty() || cell.isWall()) {
                continue;
            }

            WorldCohesionCalculator::COMCohesionForce com_cohesion = {
                { 0.0, 0.0 }, 0.0, { 0.0, 0.0 }, 0, 0.0, 0.0, false
            };

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
            BoundaryCrossings crossed_boundaries =
                pImpl->collision_calculator_.getAllBoundaryCrossings(newCOM);

            if (!crossed_boundaries.empty()) {
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
                    // Create enhanced MaterialMove with collision physics and COM cohesion
                    // pImpl->data_.
                    MaterialMove move = pImpl->collision_calculator_.createCollisionAwareMove(
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

                    pImpl->collision_calculator_.applyBoundaryReflection(cell, direction);
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

    return moves;
}

void World::processMaterialMoves()
{
    ScopeTimer timer(pImpl->timers_, "process_moves");

    // Shuffle moves to handle conflicts randomly.
    std::shuffle(pImpl->pending_moves_.begin(), pImpl->pending_moves_.end(), *rng_);

    for (const auto& move : pImpl->pending_moves_) {
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
        if (pImpl->physicsSettings_.swap_enabled) {
            Vector2i direction(move.toX - move.fromX, move.toY - move.fromY);
            if (pImpl->collision_calculator_.shouldSwapMaterials(
                    *this, move.fromX, move.fromY, fromCell, toCell, direction, move)) {
                TreeId from_organism = fromCell.organism_id;
                TreeId to_organism = toCell.organism_id;

                pImpl->collision_calculator_.swapCounterMovingMaterials(
                    fromCell, toCell, direction, move);

                if (from_organism != INVALID_TREE_ID) {
                    pImpl->organism_transfers_.push_back(
                        { Vector2i{ static_cast<int>(move.fromX), static_cast<int>(move.fromY) },
                          Vector2i{ static_cast<int>(move.toX), static_cast<int>(move.toY) },
                          from_organism,
                          fromCell.fill_ratio });
                }

                if (to_organism != INVALID_TREE_ID) {
                    pImpl->organism_transfers_.push_back(
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
                pImpl->collision_calculator_.handleTransferMove(*this, fromCell, toCell, move);
                break;
            case CollisionType::ELASTIC_REFLECTION:
                pImpl->collision_calculator_.handleElasticCollision(fromCell, toCell, move);
                break;
            case CollisionType::INELASTIC_COLLISION:
                pImpl->collision_calculator_.handleInelasticCollision(
                    *this, fromCell, toCell, move);
                break;
            case CollisionType::FRAGMENTATION:
                pImpl->collision_calculator_.handleFragmentation(*this, fromCell, toCell, move);
                break;
            case CollisionType::ABSORPTION:
                pImpl->collision_calculator_.handleAbsorption(*this, fromCell, toCell, move);
                break;
        }

        // Record organism transfer if material had organism ownership.
        if (organism_id != 0 && move.collision_type == CollisionType::TRANSFER_ONLY) {
            // Transfer occurred - record it for TreeManager update.
            recordOrganismTransfer(
                move.fromX, move.fromY, move.toX, move.toY, organism_id, move.amount);
        }
    }

    pImpl->pending_moves_.clear();

    // Notify TreeManager of all organism transfers for efficient tracking updates.
    if (!pImpl->organism_transfers_.empty() && tree_manager_) {
        tree_manager_->notifyTransfers(pImpl->organism_transfers_);
        pImpl->organism_transfers_.clear();
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
        at(x, 0).replaceMaterial(MaterialType::WALL, 1.0);
        at(x, pImpl->data_.height - 1).replaceMaterial(MaterialType::WALL, 1.0);
    }

    // Left and right walls.
    for (uint32_t y = 0; y < pImpl->data_.height; ++y) {
        at(0, y).replaceMaterial(MaterialType::WALL, 1.0);
        at(pImpl->data_.width - 1, y).replaceMaterial(MaterialType::WALL, 1.0);
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

size_t World::coordToIndex(uint32_t x, uint32_t y) const
{
    return y * pImpl->data_.width + x;
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
        for (uint32_t x = 0; x < pImpl->data_.width; ++x) {
            at(x, 0).clear();                       // Top wall.
            at(x, pImpl->data_.height - 1).clear(); // Bottom wall.
        }
        for (uint32_t y = 0; y < pImpl->data_.height; ++y) {
            at(0, y).clear();                      // Left wall.
            at(pImpl->data_.width - 1, y).clear(); // Right wall.
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
    pImpl->physicsSettings_.pressure_hydrostatic_strength = strength;
    spdlog::info("Hydrostatic pressure strength set to {:.2f}", strength);
}

double World::getHydrostaticPressureStrength() const
{
    return pImpl->physicsSettings_.pressure_hydrostatic_strength;
}

void World::setDynamicPressureStrength(double strength)
{
    pImpl->physicsSettings_.pressure_dynamic_strength = strength;
    spdlog::info("Dynamic pressure strength set to {:.2f}", strength);
}

double World::getDynamicPressureStrength() const
{
    return pImpl->physicsSettings_.pressure_dynamic_strength;
}

std::string World::settingsToString() const
{
    std::stringstream ss;
    ss << "=== World Settings ===\n";
    ss << "Grid size: " << pImpl->data_.width << "x" << pImpl->data_.height << "\n";
    ss << "Gravity: " << pImpl->physicsSettings_.gravity << "\n";
    ss << "Hydrostatic pressure enabled: " << (isHydrostaticPressureEnabled() ? "true" : "false")
       << "\n";
    ss << "Dynamic pressure enabled: " << (isDynamicPressureEnabled() ? "true" : "false") << "\n";
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
