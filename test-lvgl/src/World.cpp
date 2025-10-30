#include "World.h"
#include "Cell.h"
#include "ScopeTimer.h"
#include "SimulatorUI.h"
#include "Vector2i.h"
#include "WorldAirResistanceCalculator.h"
#include "WorldCohesionCalculator.h"
#include "WorldCollisionCalculator.h"
#include "WorldSupportCalculator.h"
#include "WorldInterpolationTool.h"
#include "spdlog/spdlog.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <queue>
#include <random>
#include <set>
#include <sstream>

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
      pressure_system_(PressureSystem::Original),
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
      floating_particle_(MaterialType::AIR, 0.0),
      floating_particle_pixel_x_(0.0),
      floating_particle_pixel_y_(0.0),
      dragged_velocity_(0.0, 0.0),
      dragged_com_(0.0, 0.0),
      selected_material_(MaterialType::DIRT),
      support_calculator_(*this),
      pressure_calculator_(*this),
      collision_calculator_(*this),
      adhesion_calculator_(*this),
      ui_ref_(nullptr)
{
    spdlog::info("Creating World: {}x{} grid with pure-material physics", width_, height_);

    // Initialize cell grid.
    cells_.resize(width_ * height_);

    // Initialize with air.
    for (auto& cell : cells_) {
        cell = Cell(MaterialType::AIR, 0.0);
    }

    // Set up boundary walls if enabled.
    if (areWallsEnabled()) {
        setupBoundaryWalls();
    }

    timers_.startTimer("total_simulation");

    // Initialize WorldSetup using base class method.
    initializeWorldSetup();

    spdlog::info("World initialization complete");
}

World::~World()
{
    spdlog::info("Destroying World: {}x{} grid", width_, height_);
    timers_.stopTimer("total_simulation");
    timers_.dumpTimerStats();
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
    if (add_particles_enabled_ && worldSetup_) {
        ScopeTimer addParticlesTimer(timers_, "add_particles");
        worldSetup_->addParticles(*this, timestep_, deltaTimeSeconds);
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

void World::draw(lv_obj_t& drawArea)
{
    ScopeTimer timer(timers_, "draw");

    spdlog::trace("World::draw() - rendering {} cells", cells_.size());

    for (uint32_t y = 0; y < height_; y++) {
        for (uint32_t x = 0; x < width_; x++) {
            at(x, y).draw(&drawArea, x, y, debug_draw_enabled_);
        }
    }

    // Draw floating particle if dragging.
    if (has_floating_particle_ && last_drag_cell_x_ >= 0 && last_drag_cell_y_ >= 0
        && isValidCell(last_drag_cell_x_, last_drag_cell_y_)) {
        // Render floating particle at current drag position.
        // This particle can potentially collide with other objects in the world.
        floating_particle_.draw(
            &drawArea, last_drag_cell_x_, last_drag_cell_y_, debug_draw_enabled_);
        spdlog::trace(
            "Drew floating particle {} at cell ({},{}) pixel pos ({:.1f},{:.1f})",
            getMaterialName(floating_particle_.getMaterialType()),
            last_drag_cell_x_,
            last_drag_cell_y_,
            floating_particle_pixel_x_,
            floating_particle_pixel_y_);
    }
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
    // Use the base class implementation for standard setup.
    WorldInterface::setup();

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
        dragged_material_ = cell.getMaterialType();
        dragged_amount_ = cell.getFillRatio();

        // Initialize drag position tracking.
        last_drag_cell_x_ = -1;
        last_drag_cell_y_ = -1;

        // Initialize velocity tracking.
        recent_positions_.clear();
        recent_positions_.push_back({ pixelX, pixelY });
        dragged_velocity_ = Vector2d(0.0, 0.0);

        // Calculate sub-cell COM position.
        double subCellX = (pixelX % Cell::WIDTH) / static_cast<double>(Cell::WIDTH);
        double subCellY = (pixelY % Cell::HEIGHT) / static_cast<double>(Cell::HEIGHT);
        dragged_com_ = Vector2d(subCellX * 2.0 - 1.0, subCellY * 2.0 - 1.0);

        // Create floating particle for drag interaction.
        has_floating_particle_ = true;
        floating_particle_.setMaterialType(dragged_material_);
        floating_particle_.setFillRatio(dragged_amount_);
        floating_particle_.setCOM(dragged_com_);
        floating_particle_.setVelocity(dragged_velocity_);
        floating_particle_pixel_x_ = static_cast<double>(pixelX);
        floating_particle_pixel_y_ = static_cast<double>(pixelY);

        // Remove material from source cell.
        cell.clear();
        cell.markDirty();

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
    dragged_com_ = Vector2d(subCellX * 2.0 - 1.0, subCellY * 2.0 - 1.0);

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
            floating_particle_.setVelocity(Vector2d(dx, dy));

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
    dragged_velocity_ = Vector2d(0.0, 0.0);
    if (recent_positions_.size() >= 2) {
        const auto& first = recent_positions_[0];
        const auto& last = recent_positions_.back();

        double dx = static_cast<double>(last.first - first.first);
        double dy = static_cast<double>(last.second - first.second);

        // Scale velocity based on Cell dimensions (similar to WorldA).
        dragged_velocity_ = Vector2d(dx / (Cell::WIDTH * 2.0), dy / (Cell::HEIGHT * 2.0));

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
        targetCell.setMaterialType(dragged_material_);
        targetCell.setFillRatio(dragged_amount_);
        targetCell.setCOM(dragged_com_);
        targetCell.setVelocity(dragged_velocity_);
        targetCell.markDirty();

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
    dragged_velocity_ = Vector2d(0.0, 0.0);
    dragged_com_ = Vector2d(0.0, 0.0);
}

void World::restoreLastDragCell()
{
    if (!is_dragging_) {
        return;
    }

    // Restore material to the original drag start location.
    if (isValidCell(drag_start_x_, drag_start_y_)) {
        Cell& originCell = at(drag_start_x_, drag_start_y_);
        originCell.setMaterialType(dragged_material_);
        originCell.setFillRatio(dragged_amount_);
        originCell.markDirty();
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
    dragged_velocity_ = Vector2d(0.0, 0.0);
    dragged_com_ = Vector2d(0.0, 0.0);
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

void World::markAllCellsDirty()
{
    for (auto& cell : cells_) {
        cell.markDirty();
    }
}

// =================================================================.
// UI INTEGRATION.
// =================================================================.

void World::setUI(std::unique_ptr<SimulatorUI> ui)
{
    ui_ = std::move(ui);
    spdlog::debug("UI set for World");
}

void World::setUIReference(SimulatorUI* ui)
{
    ui_ref_ = ui;
    spdlog::debug("UI reference set for World");
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

CellInterface& World::getCellInterface(uint32_t x, uint32_t y)
{
    return at(x, y); // Call the existing at() method.
}

const CellInterface& World::getCellInterface(uint32_t x, uint32_t y) const
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
            if (nonEmptyCells <= 5) { // Log first 5 non-empty cells.
                spdlog::info(
                    "DEBUGGING: Cell {} has mass={:.3f} material={} fill_ratio={:.3f}",
                    cellCount - 1,
                    cellMass,
                    static_cast<int>(cell.getMaterialType()),
                    cell.getFillRatio());
            }
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
            double total_pressure = cell.getPressure();
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
    double base_multiplier;
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

    // Now resolve all accumulated forces using viscosity model.
    WorldSupportCalculator support_calc(*this);

    for (uint32_t y = 0; y < height_; ++y) {
        for (uint32_t x = 0; x < width_; ++x) {
            Cell& cell = at(x, y);

            if (cell.isEmpty() || cell.isWall()) {
                continue;
            }

            // Get the total pending force (net driving force).
            Vector2d net_driving_force = cell.getPendingForce();

            // Get material properties.
            const MaterialProperties& props = getMaterialProperties(cell.getMaterialType());

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
            double velocity_magnitude = cell.getVelocity().magnitude();
            double friction_coefficient = getFrictionCoefficient(velocity_magnitude, props);

            // Apply global friction strength multiplier.
            friction_coefficient = 1.0 + (friction_coefficient - 1.0) * friction_strength_;

            // Cache the friction coefficient for visualization.
            cell.setCachedFrictionCoefficient(friction_coefficient);

            // Combine viscosity with friction and motion state.
            double effective_viscosity =
                props.viscosity * friction_coefficient * motion_multiplier * 1000;

            // Apply continuous damping with friction (no thresholds).
            double damping_factor = 1.0
                + (effective_viscosity * viscosity_strength_ * cell.getFillRatio()
                   * support_factor);

            // Ensure damping factor is never zero or negative to prevent division by zero.
            if (damping_factor <= 0.0) {
                damping_factor = 0.001; // Minimal damping to prevent division by zero.
            }

            // Store damping info for visualization (X=friction_coefficient, Y=damping_factor).
            // Only store if viscosity is actually enabled and having an effect.
            if (viscosity_strength_ > 0.0 && props.viscosity > 0.0) {
                cell.setAccumulatedCohesionForce(Vector2d(friction_coefficient, damping_factor));
            }
            else {
                cell.setAccumulatedCohesionForce(
                    Vector2d(0.0, 0.0)); // Clear when viscosity is off.
            }

            // Calculate velocity change from forces.
            Vector2d velocity_change = net_driving_force / damping_factor * deltaTime;

            // Update velocity.
            Vector2d new_velocity = cell.getVelocity() + velocity_change;
            cell.setVelocity(new_velocity);

            // Debug logging.
            if (net_driving_force.mag() > 0.001) {
                spdlog::debug(
                    "Cell ({},{}) {} - Force: ({:.3f},{:.3f}), viscosity: {:.3f}, "
                    "friction: {:.3f}, support: {:.1f}, motion_mult: {:.3f}, damping: {:.3f}, "
                    "vel_change: ({:.3f},{:.3f}), new_vel: ({:.3f},{:.3f})",
                    x,
                    y,
                    getMaterialName(cell.getMaterialType()),
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
            cell.setAccumulatedAdhesionForce(
                adhesion.force_direction * effective_adhesion_magnitude);
            cell.setAccumulatedCOMCohesionForce(
                com_cohesion.force_direction * effective_com_cohesion_magnitude);

            // NOTE: Force calculation and resistance checking now handled in resolveForces().
            // This method only needs to handle material movement based on COM positions.

            // Debug: Check if cell has any velocity or interesting COM.
            Vector2d current_velocity = cell.getVelocity();
            Vector2d oldCOM = cell.getCOM();
            if (current_velocity.length() > 0.01 || std::abs(oldCOM.x) > 0.5
                || std::abs(oldCOM.y) > 0.5) {
                spdlog::debug(
                    "Cell ({},{}) {} - Velocity: ({:.3f},{:.3f}), COM: ({:.3f},{:.3f})",
                    x,
                    y,
                    getMaterialName(cell.getMaterialType()),
                    current_velocity.x,
                    current_velocity.y,
                    oldCOM.x,
                    oldCOM.y);
            }

            // Update COM based on velocity (with proper deltaTime integration).
            Vector2d newCOM = cell.getCOM() + cell.getVelocity() * deltaTime;

            // Enhanced: Check if COM crosses any boundary [-1,1] for universal collision detection.
            std::vector<Vector2i> crossed_boundaries =
                collision_calculator_.getAllBoundaryCrossings(newCOM);

            if (!crossed_boundaries.empty()) {
                spdlog::debug(
                    "Boundary crossings detected for {} at ({},{}) with COM ({:.2f},{:.2f}) -> {} "
                    "crossings",
                    getMaterialName(cell.getMaterialType()),
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
                            getMaterialName(at(targetPos).getMaterialType()),
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
                        getMaterialName(cell.getMaterialType()),
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
                Vector2d currentCOM = cell.getCOM();
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
            if (toCell.getMaterialType() == MaterialType::WALL) {
                fromCell.setDynamicPressure(
                    fromCell.getDynamicPressure() + move.pressure_from_excess);

                spdlog::debug(
                    "Wall blocked transfer: source cell({},{}) pressure increased by {:.3f}",
                    move.fromX,
                    move.fromY,
                    move.pressure_from_excess);
            }
            else {
                // Normal materials receive the pressure.
                toCell.setDynamicPressure(toCell.getDynamicPressure() + move.pressure_from_excess);

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
                getMaterialName(toCell.getMaterialType()),
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
    ConfigurableWorldSetup* configSetup = dynamic_cast<ConfigurableWorldSetup*>(worldSetup_.get());
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
    const ConfigurableWorldSetup* configSetup =
        dynamic_cast<const ConfigurableWorldSetup*>(worldSetup_.get());
    return configSetup ? configSetup->areWallsEnabled() : false;
}

void World::setHydrostaticPressureEnabled(bool enabled)
{
    // Backward compatibility: set strength to 0 (disabled) or default (enabled).
    hydrostatic_pressure_strength_ = enabled ? 1.0 : 0.0;

    spdlog::info("Clearing all pressure values");
    for (auto& cell : cells_) {
        cell.setHydrostaticPressure(0.0);
        if (cell.getDynamicPressure() < MIN_MATTER_THRESHOLD) {
            cell.setPressureGradient(Vector2d(0.0, 0.0));
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
        if (cell.getHydrostaticPressure() < MIN_MATTER_THRESHOLD) {
            cell.setPressureGradient(Vector2d(0.0, 0.0));
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
    ss << "Pressure system: ";
    switch (pressure_system_) {
        case PressureSystem::Original:
            ss << "Original";
            break;
        case PressureSystem::TopDown:
            ss << "TopDown";
            break;
        case PressureSystem::IterativeSettling:
            ss << "IterativeSettling";
            break;
    }
    ss << "\n";
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
void World::setWorldSetup(std::unique_ptr<WorldSetup> newSetup)
{
    worldSetup_ = std::move(newSetup);
    // Reset and apply the new setup.
    if (worldSetup_) {
        this->setup();
    }
}

WorldSetup* World::getWorldSetup() const
{
    return worldSetup_.get();
}

// =================================================================
// JSON SERIALIZATION
// =================================================================

rapidjson::Document World::toJSON() const
{
    rapidjson::Document doc(rapidjson::kObjectType);
    auto& allocator = doc.GetAllocator();

    // Grid metadata.
    rapidjson::Value grid(rapidjson::kObjectType);
    grid.AddMember("width", width_, allocator);
    grid.AddMember("height", height_, allocator);
    grid.AddMember("timestep", timestep_, allocator);
    doc.AddMember("grid", grid, allocator);

    // Simulation state.
    rapidjson::Value simulation(rapidjson::kObjectType);
    simulation.AddMember("timescale", timescale_, allocator);
    simulation.AddMember("removed_mass", removed_mass_, allocator);
    doc.AddMember("simulation", simulation, allocator);

    // Physics parameters.
    rapidjson::Value physics(rapidjson::kObjectType);
    physics.AddMember("gravity", gravity_, allocator);
    physics.AddMember("elasticity_factor", elasticity_factor_, allocator);
    physics.AddMember("pressure_scale", pressure_scale_, allocator);
    physics.AddMember("water_pressure_threshold", water_pressure_threshold_, allocator);
    physics.AddMember("pressure_system", static_cast<int>(pressure_system_), allocator);
    physics.AddMember("pressure_diffusion_enabled", pressure_diffusion_enabled_, allocator);
    physics.AddMember("hydrostatic_pressure_strength", hydrostatic_pressure_strength_, allocator);
    physics.AddMember("dynamic_pressure_strength", dynamic_pressure_strength_, allocator);
    doc.AddMember("physics", physics, allocator);

    // Cohesion/adhesion/viscosity controls.
    rapidjson::Value forces(rapidjson::kObjectType);
    forces.AddMember("cohesion_bind_force_enabled", cohesion_bind_force_enabled_, allocator);
    forces.AddMember("cohesion_com_force_strength", cohesion_com_force_strength_, allocator);
    forces.AddMember("cohesion_bind_force_strength", cohesion_bind_force_strength_, allocator);
    forces.AddMember("com_cohesion_range", com_cohesion_range_, allocator);
    forces.AddMember("viscosity_strength", viscosity_strength_, allocator);
    forces.AddMember("friction_strength", friction_strength_, allocator);
    forces.AddMember("adhesion_strength", adhesion_calculator_.getAdhesionStrength(), allocator);
    forces.AddMember("adhesion_enabled", adhesion_calculator_.isAdhesionEnabled(), allocator);
    forces.AddMember("air_resistance_enabled", air_resistance_enabled_, allocator);
    forces.AddMember("air_resistance_strength", air_resistance_strength_, allocator);
    doc.AddMember("forces", forces, allocator);

    // Setup controls.
    rapidjson::Value setup(rapidjson::kObjectType);
    setup.AddMember("add_particles_enabled", add_particles_enabled_, allocator);
    setup.AddMember("debug_draw_enabled", debug_draw_enabled_, allocator);
    setup.AddMember("selected_material", materialTypeToJson(selected_material_, allocator), allocator);
    doc.AddMember("setup", setup, allocator);

    // Cells (sparse - only serialize non-empty cells).
    rapidjson::Value cells(rapidjson::kArrayType);
    for (uint32_t y = 0; y < height_; ++y) {
        for (uint32_t x = 0; x < width_; ++x) {
            const Cell& cell = at(x, y);
            // Only serialize cells with material.
            if (cell.getFillRatio() > Cell::MIN_FILL_THRESHOLD ||
                cell.getMaterialType() != MaterialType::AIR) {
                rapidjson::Value cellEntry(rapidjson::kObjectType);
                cellEntry.AddMember("x", x, allocator);
                cellEntry.AddMember("y", y, allocator);
                cellEntry.AddMember("data", cell.toJson(allocator), allocator);
                cells.PushBack(cellEntry, allocator);
            }
        }
    }
    doc.AddMember("cells", cells, allocator);

    return doc;
}

void World::fromJSON(const rapidjson::Document& doc)
{
    if (!doc.IsObject()) {
        throw std::runtime_error("World::fromJSON: JSON document must be an object");
    }

    // Parse grid metadata.
    if (!doc.HasMember("grid") || !doc["grid"].IsObject()) {
        throw std::runtime_error("World::fromJSON: Missing or invalid 'grid' section");
    }
    const auto& grid = doc["grid"];
    uint32_t width = grid["width"].GetUint();
    uint32_t height = grid["height"].GetUint();
    uint32_t timestep = grid["timestep"].GetUint();

    // Resize if necessary.
    if (width != width_ || height != height_) {
        resizeGrid(width, height);
    }

    // Clear and reset.
    reset();
    timestep_ = timestep;

    // Parse simulation state.
    if (doc.HasMember("simulation") && doc["simulation"].IsObject()) {
        const auto& simulation = doc["simulation"];
        if (simulation.HasMember("timescale")) {
            timescale_ = simulation["timescale"].GetDouble();
        }
        if (simulation.HasMember("removed_mass")) {
            removed_mass_ = simulation["removed_mass"].GetDouble();
        }
    }

    // Parse physics parameters.
    if (doc.HasMember("physics") && doc["physics"].IsObject()) {
        const auto& physics = doc["physics"];
        if (physics.HasMember("gravity")) gravity_ = physics["gravity"].GetDouble();
        if (physics.HasMember("elasticity_factor"))
            elasticity_factor_ = physics["elasticity_factor"].GetDouble();
        if (physics.HasMember("pressure_scale"))
            pressure_scale_ = physics["pressure_scale"].GetDouble();
        if (physics.HasMember("water_pressure_threshold"))
            water_pressure_threshold_ = physics["water_pressure_threshold"].GetDouble();
        if (physics.HasMember("pressure_system"))
            pressure_system_ = static_cast<PressureSystem>(physics["pressure_system"].GetInt());
        if (physics.HasMember("pressure_diffusion_enabled"))
            pressure_diffusion_enabled_ = physics["pressure_diffusion_enabled"].GetBool();
        if (physics.HasMember("hydrostatic_pressure_strength"))
            hydrostatic_pressure_strength_ = physics["hydrostatic_pressure_strength"].GetDouble();
        if (physics.HasMember("dynamic_pressure_strength"))
            dynamic_pressure_strength_ = physics["dynamic_pressure_strength"].GetDouble();
    }

    // Parse force controls.
    if (doc.HasMember("forces") && doc["forces"].IsObject()) {
        const auto& forces = doc["forces"];
        if (forces.HasMember("cohesion_bind_force_enabled"))
            cohesion_bind_force_enabled_ = forces["cohesion_bind_force_enabled"].GetBool();
        if (forces.HasMember("cohesion_com_force_strength"))
            cohesion_com_force_strength_ = forces["cohesion_com_force_strength"].GetDouble();
        if (forces.HasMember("cohesion_bind_force_strength"))
            cohesion_bind_force_strength_ = forces["cohesion_bind_force_strength"].GetDouble();
        if (forces.HasMember("com_cohesion_range"))
            com_cohesion_range_ = forces["com_cohesion_range"].GetUint();
        if (forces.HasMember("viscosity_strength"))
            viscosity_strength_ = forces["viscosity_strength"].GetDouble();
        if (forces.HasMember("friction_strength"))
            friction_strength_ = forces["friction_strength"].GetDouble();
        // Deserialize adhesion strength before enabled to avoid overwriting.
        if (forces.HasMember("adhesion_strength"))
            adhesion_calculator_.setAdhesionStrength(forces["adhesion_strength"].GetDouble());
        // Only use setAdhesionEnabled if no explicit strength was provided.
        if (forces.HasMember("adhesion_enabled") && !forces.HasMember("adhesion_strength"))
            adhesion_calculator_.setAdhesionEnabled(forces["adhesion_enabled"].GetBool());
        if (forces.HasMember("air_resistance_enabled"))
            air_resistance_enabled_ = forces["air_resistance_enabled"].GetBool();
        if (forces.HasMember("air_resistance_strength"))
            air_resistance_strength_ = forces["air_resistance_strength"].GetDouble();
    }

    // Parse setup controls.
    if (doc.HasMember("setup") && doc["setup"].IsObject()) {
        const auto& setup = doc["setup"];
        if (setup.HasMember("add_particles_enabled"))
            add_particles_enabled_ = setup["add_particles_enabled"].GetBool();
        if (setup.HasMember("debug_draw_enabled"))
            debug_draw_enabled_ = setup["debug_draw_enabled"].GetBool();
        if (setup.HasMember("selected_material"))
            selected_material_ = materialTypeFromJson(setup["selected_material"]);
    }

    // Parse cells.
    if (!doc.HasMember("cells") || !doc["cells"].IsArray()) {
        throw std::runtime_error("World::fromJSON: Missing or invalid 'cells' array");
    }

    const auto& cells = doc["cells"];
    for (rapidjson::SizeType i = 0; i < cells.Size(); ++i) {
        const auto& cellEntry = cells[i];
        if (!cellEntry.IsObject()) {
            throw std::runtime_error("World::fromJSON: Cell entry must be an object");
        }

        uint32_t x = cellEntry["x"].GetUint();
        uint32_t y = cellEntry["y"].GetUint();

        if (!isValidCell(x, y)) {
            throw std::runtime_error(
                "World::fromJSON: Invalid cell coordinates (" + std::to_string(x) + "," +
                std::to_string(y) + ")");
        }

        const auto& cellData = cellEntry["data"];
        Cell cell = Cell::fromJson(cellData);

        // Replace cell in grid.
        at(x, y) = cell;
    }

    spdlog::info(
        "World deserialized from JSON: {}x{} grid, {} timesteps, {} cells",
        width_,
        height_,
        timestep_,
        cells.Size());
}
