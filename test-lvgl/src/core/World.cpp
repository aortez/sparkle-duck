#include "World.h"
#include "Cell.h"
#include "ScopeTimer.h"
#include "Vector2i.h"
#include "WorldAirResistanceCalculator.h"
#include "WorldCohesionCalculator.h"
#include "WorldCollisionCalculator.h"
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
    : width_(width),
      height_(height),
      timestep_(0),
      timescale_(1.0),
      removed_mass_(0.0),
      gravity_(9.81),
      elasticity_factor_(0.8),
      pressure_scale_(1.0),
      water_pressure_threshold_(0.0004),
      pressure_diffusion_enabled_(false),
      hydrostatic_pressure_strength_(0.0),
      dynamic_pressure_strength_(0.0),
      add_particles_enabled_(true),
      debug_draw_enabled_(true),
      cohesion_bind_force_enabled_(false),

      cohesion_com_force_strength_(0.0),
      cohesion_bind_force_strength_(1.0),
      com_cohesion_range_(1),
      viscosity_strength_(1.0),
      friction_strength_(1.0),
      air_resistance_enabled_(true),
      air_resistance_strength_(0.1),
      is_dragging_(false),
      drag_start_x_(-1),
      drag_start_y_(-1),
      dragged_material_(MaterialType::AIR),
      dragged_amount_(0.0),
      last_drag_cell_x_(-1),
      last_drag_cell_y_(-1),
      has_floating_particle_(false),
      floating_particle_{MaterialType::AIR, 0.0},
      floating_particle_pixel_x_(0.0),
      floating_particle_pixel_y_(0.0),
      dragged_velocity_(0.0, 0.0),
      dragged_com_(0.0, 0.0),
      selected_material_(MaterialType::DIRT),
      support_calculator_(*this),
      pressure_calculator_(*this),
      collision_calculator_(*this),
      adhesion_calculator_(*this),
      friction_calculator_(*this),
      worldEventGenerator_(nullptr)
{
    spdlog::info("Creating World: {}x{} grid with pure-material physics", width_, height_);

    // Initialize cell grid.
    cells_.resize(width_ * height_);

    // Initialize with air.
    for (auto& cell : cells_) {
        cell = Cell{MaterialType::AIR, 0.0};
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
    spdlog::info("Destroying World: {}x{} grid", width_, height_);
    timers_.stopTimer("total_simulation");
    timers_.dumpTimerStats();
}

World::World(const World& other)
    : cells_(other.cells_),
      width_(other.width_),
      height_(other.height_),
      timestep_(other.timestep_),
      timescale_(other.timescale_),
      removed_mass_(other.removed_mass_),
      gravity_(other.gravity_),
      elasticity_factor_(other.elasticity_factor_),
      pressure_scale_(other.pressure_scale_),
      water_pressure_threshold_(other.water_pressure_threshold_),
      pressure_diffusion_enabled_(other.pressure_diffusion_enabled_),
      hydrostatic_pressure_strength_(other.hydrostatic_pressure_strength_),
      dynamic_pressure_strength_(other.dynamic_pressure_strength_),
      add_particles_enabled_(other.add_particles_enabled_),
      debug_draw_enabled_(other.debug_draw_enabled_),
      cohesion_bind_force_enabled_(other.cohesion_bind_force_enabled_),
      cohesion_com_force_strength_(other.cohesion_com_force_strength_),
      cohesion_bind_force_strength_(other.cohesion_bind_force_strength_),
      com_cohesion_range_(other.com_cohesion_range_),
      viscosity_strength_(other.viscosity_strength_),
      friction_strength_(other.friction_strength_),
      air_resistance_enabled_(other.air_resistance_enabled_),
      air_resistance_strength_(other.air_resistance_strength_),
      is_dragging_(false), // Don't copy drag state.
      drag_start_x_(-1),
      drag_start_y_(-1),
      dragged_material_(MaterialType::AIR),
      dragged_amount_(0.0),
      last_drag_cell_x_(-1),
      last_drag_cell_y_(-1),
      has_floating_particle_(false),
      floating_particle_{MaterialType::AIR, 0.0},
      floating_particle_pixel_x_(0.0),
      floating_particle_pixel_y_(0.0),
      dragged_velocity_(0.0, 0.0),
      dragged_com_(0.0, 0.0),
      selected_material_(other.selected_material_),
      timers_(),
      support_calculator_(*this),
      pressure_calculator_(*this),
      collision_calculator_(*this),
      adhesion_calculator_(*this),
      friction_calculator_(*this),
      worldEventGenerator_(other.worldEventGenerator_ ? other.worldEventGenerator_->clone() : nullptr)
{
    spdlog::info("Copied World: {}x{} grid", width_, height_);
}

World& World::operator=(const World& other)
{
    if (this != &other) {
        cells_ = other.cells_;
        width_ = other.width_;
        height_ = other.height_;
        timestep_ = other.timestep_;
        timescale_ = other.timescale_;
        removed_mass_ = other.removed_mass_;
        gravity_ = other.gravity_;
        elasticity_factor_ = other.elasticity_factor_;
        pressure_scale_ = other.pressure_scale_;
        water_pressure_threshold_ = other.water_pressure_threshold_;
        pressure_diffusion_enabled_ = other.pressure_diffusion_enabled_;
        hydrostatic_pressure_strength_ = other.hydrostatic_pressure_strength_;
        dynamic_pressure_strength_ = other.dynamic_pressure_strength_;
        add_particles_enabled_ = other.add_particles_enabled_;
        debug_draw_enabled_ = other.debug_draw_enabled_;
        cohesion_bind_force_enabled_ = other.cohesion_bind_force_enabled_;
        cohesion_com_force_strength_ = other.cohesion_com_force_strength_;
        cohesion_bind_force_strength_ = other.cohesion_bind_force_strength_;
        com_cohesion_range_ = other.com_cohesion_range_;
        viscosity_strength_ = other.viscosity_strength_;
        friction_strength_ = other.friction_strength_;
        air_resistance_enabled_ = other.air_resistance_enabled_;
        air_resistance_strength_ = other.air_resistance_strength_;
        selected_material_ = other.selected_material_;
        // Don't copy drag or UI state.
    }
    return *this;
}

// =================================================================.
// CORE SIMULATION METHODS.
// =================================================================.

void World::advanceTime(double deltaTimeSeconds)
{
    ScopeTimer timer(timers_, "advance_time");

    const double scaledDeltaTime = deltaTimeSeconds * timescale_;
    spdlog::trace(
        "World::advanceTime: deltaTime={:.4f}s, timestep={}", deltaTimeSeconds, timestep_);
    if (scaledDeltaTime <= 0.0) {
        return;
    }

    // Add particles if enabled.
    if (add_particles_enabled_ && worldEventGenerator_) {
        ScopeTimer addParticlesTimer(timers_, "add_particles");
        worldEventGenerator_->addParticles(*this, timestep_, deltaTimeSeconds);
    }

    // Accumulate and apply all forces based on resistance.
    // This now includes pressure forces from the previous frame.
    resolveForces(scaledDeltaTime);

    processVelocityLimiting(scaledDeltaTime);

    updateTransfers(scaledDeltaTime);

    // Process queued material moves - this detects NEW blocked transfers.
    processMaterialMoves();

    // Calculate pressures for NEXT frame after moves are complete.
    // This follows the two-frame model where pressure calculated in frame N
    // affects velocities in frame N+1.
    if (hydrostatic_pressure_strength_ > 0.0) {
        pressure_calculator_.calculateHydrostaticPressure();
    }

    // Process any blocked transfers that were queued during processMaterialMoves.
    if (dynamic_pressure_strength_ > 0.0) {
        // Generate virtual gravity transfers to create pressure from gravity forces.
        // This allows dynamic pressure to model hydrostatic-like behavior.
        //        pressure_calculator_.generateVirtualGravityTransfers(scaledDeltaTime);

        pressure_calculator_.processBlockedTransfers(pressure_calculator_.blocked_transfers_);
        pressure_calculator_.blocked_transfers_.clear();
    }

    // Apply pressure diffusion before decay.
    if (pressure_diffusion_enabled_) {
        pressure_calculator_.applyPressureDiffusion(scaledDeltaTime);
    }

    // Apply pressure decay after material moves.
    pressure_calculator_.applyPressureDecay(scaledDeltaTime);

    timestep_++;
}

void World::reset()
{
    spdlog::info("Resetting World to empty state");

    timestep_ = 0;
    removed_mass_ = 0.0;
    pending_moves_.clear();

    // Clear all cells to air.
    for (auto& cell : cells_) {
        cell.clear();
    }

    spdlog::info("World reset complete - world is now empty");
}

void World::setup()
{
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

void World::addDirtAtPixel(int pixelX, int pixelY)
{
    int cellX, cellY;
    pixelToCell(pixelX, pixelY, cellX, cellY);

    spdlog::info(
        "DEBUGGING: World::addDirtAtPixel pixel ({},{}) -> cell ({},{}) valid={}",
        pixelX,
        pixelY,
        cellX,
        cellY,
        isValidCell(cellX, cellY));

    if (isValidCell(cellX, cellY)) {
        addMaterialAtCell(
            static_cast<uint32_t>(cellX), static_cast<uint32_t>(cellY), MaterialType::DIRT, 1.0);
        Cell& cell __attribute__((unused)) =
            at(static_cast<uint32_t>(cellX), static_cast<uint32_t>(cellY));
    }
}

void World::addWaterAtPixel(int pixelX, int pixelY)
{
    int cellX, cellY;
    pixelToCell(pixelX, pixelY, cellX, cellY);

    if (isValidCell(cellX, cellY)) {
        addMaterialAtCell(cellX, cellY, MaterialType::WATER, 1.0);
        spdlog::debug("Added WATER at pixel ({},{}) -> cell ({},{})", pixelX, pixelY, cellX, cellY);
    }
}

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

bool World::hasMaterialAtPixel(int pixelX, int pixelY) const
{
    int cellX, cellY;
    pixelToCell(pixelX, pixelY, cellX, cellY);

    if (isValidCell(cellX, cellY)) {
        const Cell& cell = at(cellX, cellY);
        return !cell.isEmpty();
    }

    return false;
}

// =================================================================.
// DRAG INTERACTION (SIMPLIFIED).
// =================================================================.

void World::startDragging(int pixelX, int pixelY)
{
    int cellX, cellY;
    pixelToCell(pixelX, pixelY, cellX, cellY);

    if (!isValidCell(cellX, cellY)) {
        return;
    }

    Cell& cell = at(cellX, cellY);

    if (!cell.isEmpty()) {
        is_dragging_ = true;
        drag_start_x_ = cellX;
        drag_start_y_ = cellY;
        dragged_material_ = cell.material_type;
        dragged_amount_ = cell.fill_ratio;

        // Initialize drag position tracking.
        last_drag_cell_x_ = -1;
        last_drag_cell_y_ = -1;

        // Initialize velocity tracking.
        recent_positions_.clear();
        recent_positions_.push_back({ pixelX, pixelY });
        dragged_velocity_ = Vector2d{0.0, 0.0};

        // Calculate sub-cell COM position.
        double subCellX = (pixelX % Cell::WIDTH) / static_cast<double>(Cell::WIDTH);
        double subCellY = (pixelY % Cell::HEIGHT) / static_cast<double>(Cell::HEIGHT);
        dragged_com_ = Vector2d{subCellX * 2.0 - 1.0, subCellY * 2.0 - 1.0};

        // Create floating particle for drag interaction.
        has_floating_particle_ = true;
        floating_particle_.material_type = dragged_material_;
        floating_particle_.setFillRatio(dragged_amount_);
        floating_particle_.setCOM(dragged_com_);
        floating_particle_.velocity = dragged_velocity_;
        floating_particle_pixel_x_ = static_cast<double>(pixelX);
        floating_particle_pixel_y_ = static_cast<double>(pixelY);

        // Remove material from source cell.
        cell.clear();

        spdlog::debug(
            "Started dragging {} from cell ({},{}) with COM ({:.2f},{:.2f})",
            getMaterialName(dragged_material_),
            cellX,
            cellY,
            dragged_com_.x,
            dragged_com_.y);
    }
}

void World::updateDrag(int pixelX, int pixelY)
{
    if (!is_dragging_) {
        return;
    }

    // Add position to recent history for velocity tracking.
    recent_positions_.push_back({ pixelX, pixelY });
    if (recent_positions_.size() > 5) {
        recent_positions_.erase(recent_positions_.begin());
    }

    // Update COM based on sub-cell position.
    double subCellX = (pixelX % Cell::WIDTH) / static_cast<double>(Cell::WIDTH);
    double subCellY = (pixelY % Cell::HEIGHT) / static_cast<double>(Cell::HEIGHT);
    dragged_com_ = Vector2d{subCellX * 2.0 - 1.0, subCellY * 2.0 - 1.0};

    // Update floating particle position and physics properties.
    pixelToCell(pixelX, pixelY, last_drag_cell_x_, last_drag_cell_y_);
    floating_particle_pixel_x_ = static_cast<double>(pixelX);
    floating_particle_pixel_y_ = static_cast<double>(pixelY);

    // Update floating particle properties for collision detection.
    if (has_floating_particle_) {
        floating_particle_.setCOM(dragged_com_);

        // Calculate current velocity for collision physics.
        if (recent_positions_.size() >= 2) {
            const auto& prev = recent_positions_[recent_positions_.size() - 2];
            double dx = static_cast<double>(pixelX - prev.first) / Cell::WIDTH;
            double dy = static_cast<double>(pixelY - prev.second) / Cell::HEIGHT;
            floating_particle_.velocity = Vector2d{dx, dy};

            // Check for collisions with the target cell.
            if (collision_calculator_.checkFloatingParticleCollision(
                    last_drag_cell_x_, last_drag_cell_y_, floating_particle_)) {
                Cell& targetCell = at(last_drag_cell_x_, last_drag_cell_y_);
                collision_calculator_.handleFloatingParticleCollision(
                    last_drag_cell_x_, last_drag_cell_y_, floating_particle_, targetCell);
            }
        }
    }

    spdlog::trace(
        "Drag tracking: position ({},{}) -> cell ({},{}) with COM ({:.2f},{:.2f})",
        pixelX,
        pixelY,
        last_drag_cell_x_,
        last_drag_cell_y_,
        dragged_com_.x,
        dragged_com_.y);
}

void World::endDragging(int pixelX, int pixelY)
{
    if (!is_dragging_) {
        return;
    }

    // Calculate velocity from recent positions for "toss" behavior.
    dragged_velocity_ = Vector2d{0.0, 0.0};
    if (recent_positions_.size() >= 2) {
        const auto& first = recent_positions_[0];
        const auto& last = recent_positions_.back();

        double dx = static_cast<double>(last.first - first.first);
        double dy = static_cast<double>(last.second - first.second);

        // Scale velocity based on Cell dimensions (similar to WorldA).
        dragged_velocity_ = Vector2d{dx / (Cell::WIDTH * 2.0), dy / (Cell::HEIGHT * 2.0)};

        spdlog::debug(
            "Calculated drag velocity: ({:.2f}, {:.2f}) from {} positions",
            dragged_velocity_.x,
            dragged_velocity_.y,
            recent_positions_.size());
    }

    // No cell restoration needed since preview doesn't modify cells.

    int cellX, cellY;
    pixelToCell(pixelX, pixelY, cellX, cellY);

    if (isValidCell(cellX, cellY)) {
        // Place the material with calculated velocity and COM.
        Cell& targetCell = at(cellX, cellY);
        targetCell.material_type = dragged_material_;
        targetCell.setFillRatio(dragged_amount_);
        targetCell.setCOM(dragged_com_);
        targetCell.velocity = dragged_velocity_;

        spdlog::debug(
            "Ended drag: placed {} at cell ({},{}) with velocity ({:.2f},{:.2f})",
            getMaterialName(dragged_material_),
            cellX,
            cellY,
            dragged_velocity_.x,
            dragged_velocity_.y);
    }

    // Clear floating particle.
    has_floating_particle_ = false;
    floating_particle_.clear();
    floating_particle_pixel_x_ = 0.0;
    floating_particle_pixel_y_ = 0.0;

    // Reset all drag state.
    is_dragging_ = false;
    drag_start_x_ = -1;
    drag_start_y_ = -1;
    dragged_material_ = MaterialType::AIR;
    dragged_amount_ = 0.0;
    last_drag_cell_x_ = -1;
    last_drag_cell_y_ = -1;
    recent_positions_.clear();
    dragged_velocity_ = Vector2d{0.0, 0.0};
    dragged_com_ = Vector2d{0.0, 0.0};
}

void World::restoreLastDragCell()
{
    if (!is_dragging_) {
        return;
    }

    // Restore material to the original drag start location.
    if (isValidCell(drag_start_x_, drag_start_y_)) {
        Cell& originCell = at(drag_start_x_, drag_start_y_);
        originCell.material_type = dragged_material_;
        originCell.setFillRatio(dragged_amount_);
        spdlog::debug("Restored dragged material to origin ({},{})", drag_start_x_, drag_start_y_);
    }

    // Clear floating particle.
    has_floating_particle_ = false;
    floating_particle_.clear();
    floating_particle_pixel_x_ = 0.0;
    floating_particle_pixel_y_ = 0.0;

    // Reset drag state.
    is_dragging_ = false;
    drag_start_x_ = -1;
    drag_start_y_ = -1;
    dragged_material_ = MaterialType::AIR;
    dragged_amount_ = 0.0;
    last_drag_cell_x_ = -1;
    last_drag_cell_y_ = -1;
    recent_positions_.clear();
    dragged_velocity_ = Vector2d{0.0, 0.0};
    dragged_com_ = Vector2d{0.0, 0.0};
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
        cells_, width_, height_, newWidth, newHeight);

    // Phase 2: Update world state with the new interpolated cells.
    width_ = newWidth;
    height_ = newHeight;
    cells_ = std::move(interpolatedCells);

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
    assert(x < width_ && y < height_);
    return cells_[coordToIndex(x, y)];
}

const Cell& World::at(uint32_t x, uint32_t y) const
{
    assert(x < width_ && y < height_);
    return cells_[coordToIndex(x, y)];
}

Cell& World::at(const Vector2i& pos)
{
    return at(static_cast<uint32_t>(pos.x), static_cast<uint32_t>(pos.y));
}

const Cell& World::at(const Vector2i& pos) const
{
    return at(static_cast<uint32_t>(pos.x), static_cast<uint32_t>(pos.y));
}

Cell& World::getCell(uint32_t x, uint32_t y)
{
    return at(x, y); // Call the existing at() method.
}

const Cell& World::getCell(uint32_t x, uint32_t y) const
{
    return at(x, y); // Call the existing at() method.
}

double World::getTotalMass() const
{
    double totalMass = 0.0;
    int cellCount = 0;
    int nonEmptyCells = 0;

    for (const auto& cell : cells_) {
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

    const Vector2d gravityForce(0.0, gravity_);

    for (auto& cell : cells_) {
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

    WorldAirResistanceCalculator air_resistance_calculator(*this);

    for (uint32_t y = 0; y < height_; ++y) {
        for (uint32_t x = 0; x < width_; ++x) {
            Cell& cell = at(x, y);

            if (!cell.isEmpty() && !cell.isWall()) {
                Vector2d air_resistance_force = air_resistance_calculator.calculateAirResistance(
                    x, y, air_resistance_strength_);
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

    for (uint32_t y = 0; y < height_; ++y) {
        for (uint32_t x = 0; x < width_; ++x) {
            Cell& cell = at(x, y);

            if (cell.isEmpty() || cell.isWall()) {
                continue;
            }

            // Calculate COM cohesion force.
            WorldCohesionCalculator::COMCohesionForce com_cohesion =
                WorldCohesionCalculator(*this).calculateCOMCohesionForce(
                    x, y, com_cohesion_range_);

            // COM cohesion force accumulation (only if force is active).
            if (com_cohesion.force_active) {
                Vector2d com_cohesion_force = com_cohesion.force_direction
                    * com_cohesion.force_magnitude * cohesion_com_force_strength_;
                cell.addPendingForce(com_cohesion_force);
            }

            // Adhesion force accumulation (only if enabled).
            if (adhesion_calculator_.isAdhesionEnabled()) {
                WorldAdhesionCalculator::AdhesionForce adhesion =
                    adhesion_calculator_.calculateAdhesionForce(x, y);
                Vector2d adhesion_force = adhesion.force_direction * adhesion.force_magnitude
                    * adhesion_calculator_.getAdhesionStrength();
                cell.addPendingForce(adhesion_force);
            }
        }
    }
}

void World::applyPressureForces()
{
    if (hydrostatic_pressure_strength_ <= 0.0 && dynamic_pressure_strength_ <= 0.0) {
        return;
    }

    ScopeTimer timer(timers_, "apply_pressure_forces");

    // Apply pressure forces through the pending force system.
    for (uint32_t y = 0; y < height_; ++y) {
        for (uint32_t x = 0; x < width_; ++x) {
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
            Vector2d gradient = pressure_calculator_.calculatePressureGradient(x, y);

            // Only apply force if system is out of equilibrium.
            if (gradient.magnitude() > 0.001) {
                Vector2d pressure_force = gradient * pressure_scale_;
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
    for (auto& cell : cells_) {
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
    friction_calculator_.calculateAndApplyFrictionForces(deltaTime);

    // Now resolve all accumulated forces using viscosity model.
    WorldSupportCalculator support_calc(*this);

    for (uint32_t y = 0; y < height_; ++y) {
        for (uint32_t x = 0; x < width_; ++x) {
            Cell& cell = at(x, y);

            if (cell.isEmpty() || cell.isWall()) {
                continue;
            }

            // Get the total pending force (net driving force).
            Vector2d net_driving_force = cell.pending_force;

            // Get material properties.
            const MaterialProperties& props = getMaterialProperties(cell.material_type);

            // Calculate support factor (1.0 if supported, 0.0 if not).
            double support_factor = support_calc.hasStructuralSupport(x, y) ? 1.0 : 0.0;

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
                + (effective_viscosity * viscosity_strength_ * cell.fill_ratio
                   * support_factor);

            // Ensure damping factor is never zero or negative to prevent division by zero.
            if (damping_factor <= 0.0) {
                damping_factor = 0.001; // Minimal damping to prevent division by zero.
            }

            // Store damping info for visualization (X=friction_coefficient, Y=damping_factor).
            // Only store if viscosity is actually enabled and having an effect.
            if (viscosity_strength_ > 0.0 && props.viscosity > 0.0) {
                cell.accumulated_cohesion_force = Vector2d{friction_coefficient, damping_factor};
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
    for (auto& cell : cells_) {
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

    for (uint32_t y = 0; y < height_; ++y) {
        for (uint32_t x = 0; x < width_; ++x) {
            Cell& cell = at(x, y);

            if (cell.isEmpty() || cell.isWall()) {
                continue;
            }

            // PHASE 2: Force-Based Movement Threshold.
            // Calculate cohesion and adhesion forces before movement decisions.
            WorldCohesionCalculator::CohesionForce cohesion;
            if (cohesion_bind_force_enabled_) {
                cohesion = WorldCohesionCalculator(*this).calculateCohesionForce(x, y);
            }
            else {
                cohesion = { 0.0, 0 }; // No cohesion resistance when disabled.
            }
            WorldAdhesionCalculator::AdhesionForce adhesion =
                adhesion_calculator_.calculateAdhesionForce(x, y);

            // NEW: Calculate COM-based cohesion force.
            WorldCohesionCalculator::COMCohesionForce com_cohesion;
            if (cohesion_com_force_strength_ > 0.0) {
                com_cohesion = WorldCohesionCalculator(*this).calculateCOMCohesionForce(
                    x, y, com_cohesion_range_);
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
            cell.accumulated_adhesion_force = adhesion.force_direction * effective_adhesion_magnitude;
            cell.accumulated_com_cohesion_force = com_cohesion.force_direction * effective_com_cohesion_magnitude;

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
                fromCell.setDynamicPressure(
                    fromCell.dynamic_component + move.pressure_from_excess);

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
                collision_calculator_.handleTransferMove(fromCell, toCell, move);
                break;
            case CollisionType::ELASTIC_REFLECTION:
                collision_calculator_.handleElasticCollision(fromCell, toCell, move);
                break;
            case CollisionType::INELASTIC_COLLISION:
                collision_calculator_.handleInelasticCollision(fromCell, toCell, move);
                break;
            case CollisionType::FRAGMENTATION:
                collision_calculator_.handleFragmentation(fromCell, toCell, move);
                break;
            case CollisionType::ABSORPTION:
                collision_calculator_.handleAbsorption(fromCell, toCell, move);
                break;
        }
    }

    pending_moves_.clear();
}

void World::setupBoundaryWalls()
{
    spdlog::info("Setting up boundary walls for World");

    // Top and bottom walls.
    for (uint32_t x = 0; x < width_; ++x) {
        at(x, 0).replaceMaterial(MaterialType::WALL, 1.0);
        at(x, height_ - 1).replaceMaterial(MaterialType::WALL, 1.0);
    }

    // Left and right walls.
    for (uint32_t y = 0; y < height_; ++y) {
        at(0, y).replaceMaterial(MaterialType::WALL, 1.0);
        at(width_ - 1, y).replaceMaterial(MaterialType::WALL, 1.0);
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
    return x >= 0 && y >= 0 && static_cast<uint32_t>(x) < width_
        && static_cast<uint32_t>(y) < height_;
}

size_t World::coordToIndex(uint32_t x, uint32_t y) const
{
    return y * width_ + x;
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
        for (uint32_t x = 0; x < width_; ++x) {
            at(x, 0).clear();           // Top wall.
            at(x, height_ - 1).clear(); // Bottom wall.
        }
        for (uint32_t y = 0; y < height_; ++y) {
            at(0, y).clear();          // Left wall.
            at(width_ - 1, y).clear(); // Right wall.
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
    hydrostatic_pressure_strength_ = enabled ? 1.0 : 0.0;

    spdlog::info("Clearing all pressure values");
    for (auto& cell : cells_) {
        cell.setHydrostaticPressure(0.0);
        if (cell.dynamic_component < MIN_MATTER_THRESHOLD) {
            cell.pressure_gradient = Vector2d{0.0, 0.0};
        }
    }
}

void World::setDynamicPressureEnabled(bool enabled)
{
    // Backward compatibility: set strength to 0 (disabled) or default (enabled).
    dynamic_pressure_strength_ = enabled ? 1.0 : 0.0;

    spdlog::info("Clearing all pressure values");
    for (auto& cell : cells_) {
        cell.setDynamicPressure(0.0);
        if (cell.hydrostatic_component < MIN_MATTER_THRESHOLD) {
            cell.pressure_gradient = Vector2d{0.0, 0.0};
        }
    }

    // Clear any pending blocked transfers.
    pressure_calculator_.blocked_transfers_.clear();
}

void World::setHydrostaticPressureStrength(double strength)
{
    hydrostatic_pressure_strength_ = strength;
    spdlog::info("Hydrostatic pressure strength set to {:.2f}", strength);
}

double World::getHydrostaticPressureStrength() const
{
    return hydrostatic_pressure_strength_;
}

void World::setDynamicPressureStrength(double strength)
{
    dynamic_pressure_strength_ = strength;
    spdlog::info("Dynamic pressure strength set to {:.2f}", strength);
}

double World::getDynamicPressureStrength() const
{
    return dynamic_pressure_strength_;
}

std::string World::settingsToString() const
{
    std::stringstream ss;
    ss << "=== World Settings ===\n";
    ss << "Grid size: " << width_ << "x" << height_ << "\n";
    ss << "Gravity: " << gravity_ << "\n";
    ss << "Hydrostatic pressure enabled: " << (isHydrostaticPressureEnabled() ? "true" : "false")
       << "\n";
    ss << "Dynamic pressure enabled: " << (isDynamicPressureEnabled() ? "true" : "false") << "\n";
    ss << "Pressure scale: " << pressure_scale_ << "\n";
    ss << "Elasticity factor: " << elasticity_factor_ << "\n";
    ss << "Add particles enabled: " << (add_particles_enabled_ ? "true" : "false") << "\n";
    ss << "Walls enabled: " << (areWallsEnabled() ? "true" : "false") << "\n";
    ss << "Rain rate: " << getRainRate() << "\n";
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
void World::setWorldEventGenerator(std::unique_ptr<WorldEventGenerator> newSetup)
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
    nlohmann::json doc;

    // Grid metadata.
    doc["grid"]["width"] = width_;
    doc["grid"]["height"] = height_;
    doc["grid"]["timestep"] = timestep_;

    // Simulation state.
    doc["simulation"]["timescale"] = timescale_;
    doc["simulation"]["removed_mass"] = removed_mass_;

    // Physics parameters.
    doc["physics"]["gravity"] = gravity_;
    doc["physics"]["elasticity_factor"] = elasticity_factor_;
    doc["physics"]["pressure_scale"] = pressure_scale_;
    doc["physics"]["water_pressure_threshold"] = water_pressure_threshold_;
    doc["physics"]["pressure_diffusion_enabled"] = pressure_diffusion_enabled_;
    doc["physics"]["hydrostatic_pressure_strength"] = hydrostatic_pressure_strength_;
    doc["physics"]["dynamic_pressure_strength"] = dynamic_pressure_strength_;

    // Cohesion/adhesion/viscosity controls.
    doc["forces"]["cohesion_bind_force_enabled"] = cohesion_bind_force_enabled_;
    doc["forces"]["cohesion_com_force_strength"] = cohesion_com_force_strength_;
    doc["forces"]["cohesion_bind_force_strength"] = cohesion_bind_force_strength_;
    doc["forces"]["com_cohesion_range"] = com_cohesion_range_;
    doc["forces"]["viscosity_strength"] = viscosity_strength_;
    doc["forces"]["friction_strength"] = friction_strength_;
    doc["forces"]["adhesion_strength"] = adhesion_calculator_.getAdhesionStrength();
    doc["forces"]["adhesion_enabled"] = adhesion_calculator_.isAdhesionEnabled();
    doc["forces"]["air_resistance_enabled"] = air_resistance_enabled_;
    doc["forces"]["air_resistance_strength"] = air_resistance_strength_;

    // Setup controls.
    doc["setup"]["add_particles_enabled"] = add_particles_enabled_;
    doc["setup"]["debug_draw_enabled"] = debug_draw_enabled_;
    doc["setup"]["selected_material"] = selected_material_;  // ADL will convert MaterialType.

    // Cells (sparse - only serialize non-empty cells).
    doc["cells"] = nlohmann::json::array();
    for (uint32_t y = 0; y < height_; ++y) {
        for (uint32_t x = 0; x < width_; ++x) {
            const Cell& cell = at(x, y);
            // Only serialize cells with material.
            if (cell.fill_ratio > Cell::MIN_FILL_THRESHOLD ||
                cell.material_type != MaterialType::AIR) {
                nlohmann::json cellEntry;
                cellEntry["x"] = x;
                cellEntry["y"] = y;
                cellEntry["data"] = cell;  // ADL will convert Cell.
                doc["cells"].push_back(cellEntry);
            }
        }
    }

    return doc;
}

void World::fromJSON(const nlohmann::json& doc)
{
    if (!doc.is_object()) {
        throw std::runtime_error("World::fromJSON: JSON document must be an object");
    }

    // Parse grid metadata.
    if (!doc.contains("grid") || !doc["grid"].is_object()) {
        throw std::runtime_error("World::fromJSON: Missing or invalid 'grid' section");
    }
    const auto& grid = doc["grid"];
    uint32_t width = grid["width"].get<uint32_t>();
    uint32_t height = grid["height"].get<uint32_t>();
    uint32_t timestep = grid["timestep"].get<uint32_t>();

    // Resize if necessary.
    if (width != width_ || height != height_) {
        resizeGrid(width, height);
    }

    // Clear and reset.
    reset();
    timestep_ = timestep;

    // Parse simulation state.
    if (doc.contains("simulation") && doc["simulation"].is_object()) {
        const auto& simulation = doc["simulation"];
        if (simulation.contains("timescale")) {
            timescale_ = simulation["timescale"].get<double>();
        }
        if (simulation.contains("removed_mass")) {
            removed_mass_ = simulation["removed_mass"].get<double>();
        }
    }

    // Parse physics parameters.
    if (doc.contains("physics") && doc["physics"].is_object()) {
        const auto& physics = doc["physics"];
        if (physics.contains("gravity")) gravity_ = physics["gravity"].get<double>();
        if (physics.contains("elasticity_factor"))
            elasticity_factor_ = physics["elasticity_factor"].get<double>();
        if (physics.contains("pressure_scale"))
            pressure_scale_ = physics["pressure_scale"].get<double>();
        if (physics.contains("water_pressure_threshold"))
            water_pressure_threshold_ = physics["water_pressure_threshold"].get<double>();
        if (physics.contains("pressure_diffusion_enabled"))
            pressure_diffusion_enabled_ = physics["pressure_diffusion_enabled"].get<bool>();
        if (physics.contains("hydrostatic_pressure_strength"))
            hydrostatic_pressure_strength_ = physics["hydrostatic_pressure_strength"].get<double>();
        if (physics.contains("dynamic_pressure_strength"))
            dynamic_pressure_strength_ = physics["dynamic_pressure_strength"].get<double>();
    }

    // Parse force controls.
    if (doc.contains("forces") && doc["forces"].is_object()) {
        const auto& forces = doc["forces"];
        if (forces.contains("cohesion_bind_force_enabled"))
            cohesion_bind_force_enabled_ = forces["cohesion_bind_force_enabled"].get<bool>();
        if (forces.contains("cohesion_com_force_strength"))
            cohesion_com_force_strength_ = forces["cohesion_com_force_strength"].get<double>();
        if (forces.contains("cohesion_bind_force_strength"))
            cohesion_bind_force_strength_ = forces["cohesion_bind_force_strength"].get<double>();
        if (forces.contains("com_cohesion_range"))
            com_cohesion_range_ = forces["com_cohesion_range"].get<uint32_t>();
        if (forces.contains("viscosity_strength"))
            viscosity_strength_ = forces["viscosity_strength"].get<double>();
        if (forces.contains("friction_strength"))
            friction_strength_ = forces["friction_strength"].get<double>();
        // Deserialize adhesion strength before enabled to avoid overwriting.
        if (forces.contains("adhesion_strength"))
            adhesion_calculator_.setAdhesionStrength(forces["adhesion_strength"].get<double>());
        // Only use setAdhesionEnabled if no explicit strength was provided.
        if (forces.contains("adhesion_enabled") && !forces.contains("adhesion_strength"))
            adhesion_calculator_.setAdhesionEnabled(forces["adhesion_enabled"].get<bool>());
        if (forces.contains("air_resistance_enabled"))
            air_resistance_enabled_ = forces["air_resistance_enabled"].get<bool>();
        if (forces.contains("air_resistance_strength"))
            air_resistance_strength_ = forces["air_resistance_strength"].get<double>();
    }

    // Parse setup controls.
    if (doc.contains("setup") && doc["setup"].is_object()) {
        const auto& setup = doc["setup"];
        if (setup.contains("add_particles_enabled"))
            add_particles_enabled_ = setup["add_particles_enabled"].get<bool>();
        if (setup.contains("debug_draw_enabled"))
            debug_draw_enabled_ = setup["debug_draw_enabled"].get<bool>();
        if (setup.contains("selected_material"))
            selected_material_ = setup["selected_material"].get<MaterialType>();  // ADL conversion.
    }

    // Parse cells.
    if (!doc.contains("cells") || !doc["cells"].is_array()) {
        throw std::runtime_error("World::fromJSON: Missing or invalid 'cells' array");
    }

    const auto& cells = doc["cells"];
    for (const auto& cellEntry : cells) {
        if (!cellEntry.is_object()) {
            throw std::runtime_error("World::fromJSON: Cell entry must be an object");
        }

        uint32_t x = cellEntry["x"].get<uint32_t>();
        uint32_t y = cellEntry["y"].get<uint32_t>();

        if (!isValidCell(x, y)) {
            throw std::runtime_error(
                "World::fromJSON: Invalid cell coordinates (" + std::to_string(x) + "," +
                std::to_string(y) + ")");
        }

        const auto& cellData = cellEntry["data"];
        Cell cell = cellData.get<Cell>();  // ADL conversion.

        // Replace cell in grid.
        at(x, y) = cell;
    }

    spdlog::info(
        "World deserialized from JSON: {}x{} grid, {} timesteps, {} cells",
        width_,
        height_,
        timestep_,
        cells.size());
}

// Stub implementations for WorldInterface methods.
void World::onPreResize(uint32_t newWidth, uint32_t newHeight)
{
    spdlog::debug("World::onPreResize: {}x{} -> {}x{}", width_, height_, newWidth, newHeight);
}

bool World::shouldResize(uint32_t newWidth, uint32_t newHeight) const
{
    return width_ != newWidth || height_ != newHeight;
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
    } else if (str == "FALLING") {
        state = World::MotionState::FALLING;
    } else if (str == "SLIDING") {
        state = World::MotionState::SLIDING;
    } else if (str == "TURBULENT") {
        state = World::MotionState::TURBULENT;
    } else {
        throw std::runtime_error("Unknown MotionState: " + str);
    }
}

} // namespace DirtSim
