#include "WorldB.h"
#include "Cell.h"
#include "SimulatorUI.h"
#include "Vector2i.h"
#include "spdlog/spdlog.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <random>

WorldB::WorldB(uint32_t width, uint32_t height, lv_obj_t* draw_area)
    : width_(width),
      height_(height),
      draw_area_(draw_area),
      timestep_(0),
      timescale_(1.0),
      removed_mass_(0.0),
      gravity_(9.81),
      elasticity_factor_(0.8),
      pressure_scale_(1.0),
      water_pressure_threshold_(0.0004),
      pressure_system_(PressureSystem::Original),
      add_particles_enabled_(true),
      left_throw_enabled_(false),
      right_throw_enabled_(false),
      quadrant_enabled_(false),
      walls_enabled_(true),
      rain_rate_(0.0),
      cursor_force_enabled_(true),
      cursor_force_active_(false),
      cursor_force_x_(0),
      cursor_force_y_(0),
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
      ui_ref_(nullptr)
{
    spdlog::info("Creating WorldB: {}x{} grid with pure-material physics", width_, height_);
    
    // Initialize cell grid
    cells_.resize(width_ * height_);
    
    // Initialize with air
    for (auto& cell : cells_) {
        cell = CellB(MaterialType::AIR, 0.0);
    }
    
    // Set up boundary walls if enabled
    if (walls_enabled_) {
        setupBoundaryWalls();
    }
    
    timers_.startTimer("total_simulation");
    
    // Create configurable world setup
    world_setup_ = std::make_unique<ConfigurableWorldSetup>();
    
    spdlog::info("WorldB initialization complete");
}

WorldB::~WorldB()
{
    spdlog::info("Destroying WorldB: {}x{} grid", width_, height_);
    timers_.stopTimer("total_simulation");
    timers_.dumpTimerStats();
}

// =================================================================
// CORE SIMULATION METHODS
// =================================================================

void WorldB::advanceTime(double deltaTimeSeconds)
{
    timers_.startTimer("advance_time");
    
    spdlog::trace("WorldB::advanceTime: deltaTime={:.4f}s, timestep={}", 
                  deltaTimeSeconds, timestep_);
    
    // Add particles if enabled
    if (add_particles_enabled_ && world_setup_) {
        timers_.startTimer("add_particles");
        world_setup_->addParticles(*this, timestep_, deltaTimeSeconds);
        timers_.stopTimer("add_particles");
    }
    
    const double scaledDeltaTime = deltaTimeSeconds * timescale_;
    
    if (scaledDeltaTime > 0.0) {
        // Main physics steps
        applyGravity(scaledDeltaTime);
        processVelocityLimiting(scaledDeltaTime);
        updateTransfers(scaledDeltaTime);
        applyPressure(scaledDeltaTime);
        
        // Process queued material moves
        processMaterialMoves();
        
        timestep_++;
    }
    
    timers_.stopTimer("advance_time");
}

void WorldB::draw()
{
    if (draw_area_ == nullptr) {
        return;
    }
    
    timers_.startTimer("draw");
    
    spdlog::trace("WorldB::draw() - rendering {} cells", cells_.size());
    
    for (uint32_t y = 0; y < height_; y++) {
        for (uint32_t x = 0; x < width_; x++) {
            at(x, y).draw(draw_area_, x, y);
        }
    }
    
    // Draw floating particle if dragging
    if (has_floating_particle_ && last_drag_cell_x_ >= 0 && last_drag_cell_y_ >= 0 && 
        isValidCell(last_drag_cell_x_, last_drag_cell_y_)) {
        // Render floating particle at current drag position
        // This particle can potentially collide with other objects in the world
        floating_particle_.draw(draw_area_, last_drag_cell_x_, last_drag_cell_y_);
        spdlog::trace("Drew floating particle {} at cell ({},{}) pixel pos ({:.1f},{:.1f})", 
                      getMaterialName(floating_particle_.getMaterialType()),
                      last_drag_cell_x_, last_drag_cell_y_,
                      floating_particle_pixel_x_, floating_particle_pixel_y_);
    }
    
    timers_.stopTimer("draw");
}

void WorldB::reset()
{
    spdlog::info("Resetting WorldB to empty state");
    
    timestep_ = 0;
    removed_mass_ = 0.0;
    pending_moves_.clear();
    
    // Clear all cells to air
    for (auto& cell : cells_) {
        cell.clear();
    }
    
    spdlog::info("WorldB reset complete - world is now empty");
}

void WorldB::setup()
{
    spdlog::info("Setting up WorldB with initial materials");
    
    // First reset to empty state
    reset();
    
    spdlog::info("After reset, about to do WorldSetup");
    
    // Use the world setup strategy to initialize the world
    spdlog::info("About to check world_setup_ pointer...");
    if (world_setup_) {
        spdlog::info("Calling WorldSetup::setup for WorldB");
        world_setup_->setup(*this);
    } else {
        spdlog::error("WorldSetup is null in WorldB::setup()!");
    }
    
    // Rebuild boundary walls if enabled
    if (walls_enabled_) {
        setupBoundaryWalls();
    }
    
    spdlog::info("WorldB setup complete");
    spdlog::info("DEBUGGING: Total mass after setup = {:.3f}", getTotalMass());
}

// =================================================================
// MATERIAL ADDITION METHODS
// =================================================================

void WorldB::addDirtAtPixel(int pixelX, int pixelY)
{
    int cellX, cellY;
    pixelToCell(pixelX, pixelY, cellX, cellY);
    
    spdlog::info("DEBUGGING: WorldB::addDirtAtPixel pixel ({},{}) -> cell ({},{}) valid={}", 
                 pixelX, pixelY, cellX, cellY, isValidCell(cellX, cellY));
    
    if (isValidCell(cellX, cellY)) {
        addMaterialAtCell(static_cast<uint32_t>(cellX), static_cast<uint32_t>(cellY), MaterialType::DIRT, 1.0);
        CellB& cell __attribute__((unused)) = at(static_cast<uint32_t>(cellX), static_cast<uint32_t>(cellY));
    }
}

void WorldB::addWaterAtPixel(int pixelX, int pixelY)
{
    int cellX, cellY;
    pixelToCell(pixelX, pixelY, cellX, cellY);
    
    if (isValidCell(cellX, cellY)) {
        addMaterialAtCell(cellX, cellY, MaterialType::WATER, 1.0);
        spdlog::debug("Added WATER at pixel ({},{}) -> cell ({},{})", 
                      pixelX, pixelY, cellX, cellY);
    }
}

void WorldB::addMaterialAtCell(uint32_t x, uint32_t y, MaterialType type, double amount)
{
    if (!isValidCell(x, y)) {
        return;
    }
    
    CellB& cell = at(x, y);
    const double added = cell.addMaterial(type, amount);
    
    if (added > 0.0) {
        spdlog::trace("Added {:.3f} {} at cell ({},{})", added, getMaterialName(type), x, y);
    }
}

void WorldB::addMaterialAtPixel(int pixelX, int pixelY, MaterialType type, double amount)
{
    int cellX, cellY;
    pixelToCell(pixelX, pixelY, cellX, cellY);
    
    spdlog::debug("WorldB::addMaterialAtPixel({}) at pixel ({},{}) -> cell ({},{})", 
                  getMaterialName(type), pixelX, pixelY, cellX, cellY);
    
    if (isValidCell(cellX, cellY)) {
        addMaterialAtCell(static_cast<uint32_t>(cellX), static_cast<uint32_t>(cellY), type, amount);
    }
}

// =================================================================
// DRAG INTERACTION (SIMPLIFIED)
// =================================================================

void WorldB::startDragging(int pixelX, int pixelY)
{
    int cellX, cellY;
    pixelToCell(pixelX, pixelY, cellX, cellY);
    
    if (!isValidCell(cellX, cellY)) {
        return;
    }
    
    CellB& cell = at(cellX, cellY);
    
    if (!cell.isEmpty()) {
        is_dragging_ = true;
        drag_start_x_ = cellX;
        drag_start_y_ = cellY;
        dragged_material_ = cell.getMaterialType();
        dragged_amount_ = cell.getFillRatio();
        
        // Initialize drag position tracking
        last_drag_cell_x_ = -1;
        last_drag_cell_y_ = -1;
        
        // Initialize velocity tracking
        recent_positions_.clear();
        recent_positions_.push_back({pixelX, pixelY});
        dragged_velocity_ = Vector2d(0.0, 0.0);
        
        // Calculate sub-cell COM position
        double subCellX = (pixelX % Cell::WIDTH) / static_cast<double>(Cell::WIDTH);
        double subCellY = (pixelY % Cell::HEIGHT) / static_cast<double>(Cell::HEIGHT);
        dragged_com_ = Vector2d(subCellX * 2.0 - 1.0, subCellY * 2.0 - 1.0);
        
        // Create floating particle for drag interaction
        has_floating_particle_ = true;
        floating_particle_.setMaterialType(dragged_material_);
        floating_particle_.setFillRatio(dragged_amount_);
        floating_particle_.setCOM(dragged_com_);
        floating_particle_.setVelocity(dragged_velocity_);
        floating_particle_pixel_x_ = static_cast<double>(pixelX);
        floating_particle_pixel_y_ = static_cast<double>(pixelY);
        
        // Remove material from source cell
        cell.clear();
        cell.markDirty();
        
        spdlog::debug("Started dragging {} from cell ({},{}) with COM ({:.2f},{:.2f})", 
                      getMaterialName(dragged_material_), cellX, cellY, 
                      dragged_com_.x, dragged_com_.y);
    }
}

void WorldB::updateDrag(int pixelX, int pixelY)
{
    if (!is_dragging_) {
        return;
    }
    
    // Add position to recent history for velocity tracking
    recent_positions_.push_back({pixelX, pixelY});
    if (recent_positions_.size() > 5) {
        recent_positions_.erase(recent_positions_.begin());
    }
    
    // Update COM based on sub-cell position
    double subCellX = (pixelX % Cell::WIDTH) / static_cast<double>(Cell::WIDTH);
    double subCellY = (pixelY % Cell::HEIGHT) / static_cast<double>(Cell::HEIGHT);
    dragged_com_ = Vector2d(subCellX * 2.0 - 1.0, subCellY * 2.0 - 1.0);
    
    // Update floating particle position and physics properties
    pixelToCell(pixelX, pixelY, last_drag_cell_x_, last_drag_cell_y_);
    floating_particle_pixel_x_ = static_cast<double>(pixelX);
    floating_particle_pixel_y_ = static_cast<double>(pixelY);
    
    // Update floating particle properties for collision detection
    if (has_floating_particle_) {
        floating_particle_.setCOM(dragged_com_);
        
        // Calculate current velocity for collision physics
        if (recent_positions_.size() >= 2) {
            const auto& prev = recent_positions_[recent_positions_.size() - 2];
            double dx = static_cast<double>(pixelX - prev.first) / Cell::WIDTH;
            double dy = static_cast<double>(pixelY - prev.second) / Cell::HEIGHT;
            floating_particle_.setVelocity(Vector2d(dx, dy));
            
            // Check for collisions with the target cell
            if (checkFloatingParticleCollision(last_drag_cell_x_, last_drag_cell_y_)) {
                handleFloatingParticleCollision(last_drag_cell_x_, last_drag_cell_y_);
            }
        }
    }
    
    spdlog::trace("Drag tracking: position ({},{}) -> cell ({},{}) with COM ({:.2f},{:.2f})", 
                  pixelX, pixelY, last_drag_cell_x_, last_drag_cell_y_,
                  dragged_com_.x, dragged_com_.y);
}

void WorldB::endDragging(int pixelX, int pixelY)
{
    if (!is_dragging_) {
        return;
    }
    
    // Calculate velocity from recent positions for "toss" behavior
    dragged_velocity_ = Vector2d(0.0, 0.0);
    if (recent_positions_.size() >= 2) {
        const auto& first = recent_positions_[0];
        const auto& last = recent_positions_.back();
        
        double dx = static_cast<double>(last.first - first.first);
        double dy = static_cast<double>(last.second - first.second);
        
        // Scale velocity based on Cell dimensions (similar to WorldA)
        dragged_velocity_ = Vector2d(dx / (Cell::WIDTH * 2.0), dy / (Cell::HEIGHT * 2.0));
        
        spdlog::debug("Calculated drag velocity: ({:.2f}, {:.2f}) from {} positions",
                      dragged_velocity_.x, dragged_velocity_.y, recent_positions_.size());
    }
    
    // No cell restoration needed since preview doesn't modify cells
    
    int cellX, cellY;
    pixelToCell(pixelX, pixelY, cellX, cellY);
    
    if (isValidCell(cellX, cellY)) {
        // Place the material with calculated velocity and COM
        CellB& targetCell = at(cellX, cellY);
        targetCell.setMaterialType(dragged_material_);
        targetCell.setFillRatio(dragged_amount_);
        targetCell.setCOM(dragged_com_);
        targetCell.setVelocity(dragged_velocity_);
        targetCell.markDirty();
        
        spdlog::debug("Ended drag: placed {} at cell ({},{}) with velocity ({:.2f},{:.2f})", 
                      getMaterialName(dragged_material_), cellX, cellY,
                      dragged_velocity_.x, dragged_velocity_.y);
    }
    
    // Clear floating particle
    has_floating_particle_ = false;
    floating_particle_.clear();
    floating_particle_pixel_x_ = 0.0;
    floating_particle_pixel_y_ = 0.0;
    
    // Reset all drag state
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

void WorldB::restoreLastDragCell()
{
    if (!is_dragging_) {
        return;
    }
    
    // Restore material to the original drag start location
    if (isValidCell(drag_start_x_, drag_start_y_)) {
        CellB& originCell = at(drag_start_x_, drag_start_y_);
        originCell.setMaterialType(dragged_material_);
        originCell.setFillRatio(dragged_amount_);
        originCell.markDirty();
        spdlog::debug("Restored dragged material to origin ({},{})", drag_start_x_, drag_start_y_);
    }
    
    // Clear floating particle
    has_floating_particle_ = false;
    floating_particle_.clear();
    floating_particle_pixel_x_ = 0.0;
    floating_particle_pixel_y_ = 0.0;
    
    // Reset drag state
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

// =================================================================
// CURSOR FORCE INTERACTION
// =================================================================

void WorldB::updateCursorForce(int pixelX, int pixelY, bool isActive)
{
    cursor_force_active_ = isActive && cursor_force_enabled_;
    
    if (cursor_force_active_) {
        pixelToCell(pixelX, pixelY, cursor_force_x_, cursor_force_y_);
        spdlog::trace("Cursor force active at cell ({},{})", cursor_force_x_, cursor_force_y_);
    }
}

// =================================================================
// GRID MANAGEMENT
// =================================================================

void WorldB::resizeGrid(uint32_t newWidth, uint32_t newHeight, bool /* clearHistory */)
{
    if (newWidth == width_ && newHeight == height_) {
        return;
    }
    
    spdlog::info("Resizing WorldB grid from {}x{} to {}x{}", 
                 width_, height_, newWidth, newHeight);
    
    // Create new cell grid
    std::vector<CellB> newCells(newWidth * newHeight);
    
    // Copy existing cells (if they fit in new dimensions)
    const uint32_t copyWidth = std::min(width_, newWidth);
    const uint32_t copyHeight = std::min(height_, newHeight);
    
    for (uint32_t y = 0; y < copyHeight; ++y) {
        for (uint32_t x = 0; x < copyWidth; ++x) {
            const size_t oldIndex = y * width_ + x;
            const size_t newIndex = y * newWidth + x;
            newCells[newIndex] = cells_[oldIndex];
        }
    }
    
    // Update dimensions and swap cell storage
    width_ = newWidth;
    height_ = newHeight;
    cells_ = std::move(newCells);
    
    // Rebuild boundary walls if enabled
    if (walls_enabled_) {
        setupBoundaryWalls();
    }
    
    spdlog::info("Grid resize complete");
}

// =================================================================
// UI INTEGRATION
// =================================================================

void WorldB::setUI(std::unique_ptr<SimulatorUI> ui)
{
    ui_ = std::move(ui);
    spdlog::debug("UI set for WorldB");
}

void WorldB::setUIReference(SimulatorUI* ui)
{
    ui_ref_ = ui;
    spdlog::debug("UI reference set for WorldB");
}

// =================================================================
// WORLDB-SPECIFIC METHODS
// =================================================================

CellB& WorldB::at(uint32_t x, uint32_t y)
{
    assert(x < width_ && y < height_);
    return cells_[coordToIndex(x, y)];
}

const CellB& WorldB::at(uint32_t x, uint32_t y) const
{
    assert(x < width_ && y < height_);
    return cells_[coordToIndex(x, y)];
}

CellB& WorldB::at(const Vector2i& pos)
{
    return at(static_cast<uint32_t>(pos.x), static_cast<uint32_t>(pos.y));
}

const CellB& WorldB::at(const Vector2i& pos) const
{
    return at(static_cast<uint32_t>(pos.x), static_cast<uint32_t>(pos.y));
}

CellInterface& WorldB::getCellInterface(uint32_t x, uint32_t y)
{
    return at(x, y); // Call the existing at() method
}

const CellInterface& WorldB::getCellInterface(uint32_t x, uint32_t y) const
{
    return at(x, y); // Call the existing at() method
}

double WorldB::getTotalMass() const
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
            if (nonEmptyCells <= 5) { // Log first 5 non-empty cells
                spdlog::info("DEBUGGING: Cell {} has mass={:.3f} material={} fill_ratio={:.3f}", 
                             cellCount-1, cellMass, static_cast<int>(cell.getMaterialType()), cell.getFillRatio());
            }
        }
    }
    
    spdlog::info("DEBUGGING: WorldB total mass={:.3f} from {} cells ({} non-empty)", 
                 totalMass, cellCount, nonEmptyCells);
    return totalMass;
}

// =================================================================
// INTERNAL PHYSICS METHODS
// =================================================================

void WorldB::applyGravity(double deltaTime)
{
    timers_.startTimer("apply_gravity");
    
    const Vector2d gravityForce(0.0, gravity_ * deltaTime);
    
    for (auto& cell : cells_) {
        if (!cell.isEmpty() && !cell.isWall()) {
            Vector2d velocity = cell.getVelocity();
            velocity = velocity + gravityForce;
            cell.setVelocity(velocity);
        }
    }
    
    timers_.stopTimer("apply_gravity");
}

void WorldB::processVelocityLimiting(double deltaTime)
{
    for (auto& cell : cells_) {
        if (!cell.isEmpty()) {
            cell.limitVelocity(MAX_VELOCITY_PER_TIMESTEP, VELOCITY_DAMPING_THRESHOLD_PER_TIMESTEP, VELOCITY_DAMPING_FACTOR_PER_TIMESTEP, deltaTime);
        }
    }
}

void WorldB::updateTransfers(double deltaTime)
{
    timers_.startTimer("update_transfers");
    
    // Clear previous moves
    pending_moves_.clear();
    
    // Queue material moves based on COM positions and velocities
    queueMaterialMoves(deltaTime);
    
    timers_.stopTimer("update_transfers");
}

void WorldB::queueMaterialMoves(double deltaTime)
{
    for (uint32_t y = 0; y < height_; ++y) {
        for (uint32_t x = 0; x < width_; ++x) {
            CellB& cell = at(x, y);
            
            if (cell.isEmpty() || cell.isWall()) {
                continue;
            }
            
            // Debug: Check if cell has any velocity or interesting COM
            Vector2d velocity = cell.getVelocity();
            Vector2d oldCOM = cell.getCOM();
            if (velocity.length() > 0.01 || std::abs(oldCOM.x) > 0.5 || std::abs(oldCOM.y) > 0.5) {
                spdlog::debug("Cell ({},{}) {} - Velocity: ({:.3f},{:.3f}), COM: ({:.3f},{:.3f})", 
                              x, y, getMaterialName(cell.getMaterialType()),
                              velocity.x, velocity.y, oldCOM.x, oldCOM.y);
            }
            
            // Update COM based on velocity (with proper deltaTime integration)
            Vector2d newCOM = cell.getCOM() + cell.getVelocity() * deltaTime;
            
            // Enhanced: Check if COM crosses any boundary [-1,1] for universal collision detection
            std::vector<Vector2i> crossed_boundaries = getAllBoundaryCrossings(newCOM);
            
            if (!crossed_boundaries.empty()) {
                spdlog::debug("Boundary crossings detected for {} at ({},{}) with COM ({:.2f},{:.2f}) -> {} crossings", 
                              getMaterialName(cell.getMaterialType()), x, y, newCOM.x, newCOM.y, crossed_boundaries.size());
            }
            
            bool boundary_reflection_applied = false;
            
            for (const Vector2i& direction : crossed_boundaries) {
                Vector2i targetPos = Vector2i(x, y) + direction;
                
                if (isValidCell(targetPos)) {
                    // Create enhanced MaterialMove with collision physics
                    MaterialMove move = createCollisionAwareMove(
                        cell, at(targetPos), Vector2i(x, y), targetPos, direction, deltaTime
                    );
                    
                    // Debug logging for collision detection
                    if (move.collision_type != CollisionType::TRANSFER_ONLY) {
                        spdlog::debug("Collision detected: {} vs {} at ({},{}) -> ({},{}) - Type: {}, Energy: {:.3f}", 
                                      getMaterialName(move.material), 
                                      getMaterialName(at(targetPos).getMaterialType()),
                                      x, y, targetPos.x, targetPos.y,
                                      static_cast<int>(move.collision_type), move.collision_energy);
                    }
                    
                    pending_moves_.push_back(move);
                } else {
                    // Hit world boundary - apply elastic reflection immediately
                    spdlog::debug("World boundary hit: {} at ({},{}) direction=({},{}) - applying reflection",
                                  getMaterialName(cell.getMaterialType()), x, y, direction.x, direction.y);
                    
                    applyBoundaryReflection(cell, direction);
                    boundary_reflection_applied = true;
                }
            }
            
            // Update COM only if no boundary reflections occurred (reflection method handles COM)
            if (!boundary_reflection_applied) {
                cell.setCOM(newCOM);
            }
        }
    }
}

void WorldB::processMaterialMoves()
{
    timers_.startTimer("process_moves");
    
    // Shuffle moves to handle conflicts randomly
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::shuffle(pending_moves_.begin(), pending_moves_.end(), gen);
    
    for (const auto& move : pending_moves_) {
        CellB& fromCell = at(move.fromX, move.fromY);
        CellB& toCell = at(move.toX, move.toY);
        
        // Handle collision during the move based on collision_type
        if (move.collision_type != CollisionType::TRANSFER_ONLY) {
            spdlog::debug("Processing collision: {} vs {} at ({},{}) -> ({},{}) - Type: {}", 
                          getMaterialName(move.material), 
                          getMaterialName(toCell.getMaterialType()),
                          move.fromX, move.fromY, move.toX, move.toY,
                          static_cast<int>(move.collision_type));
        }
        
        switch (move.collision_type) {
            case CollisionType::TRANSFER_ONLY:
                handleTransferMove(fromCell, toCell, move);
                break;
            case CollisionType::ELASTIC_REFLECTION:
                handleElasticCollision(fromCell, toCell, move);
                break;
            case CollisionType::INELASTIC_COLLISION:
                handleInelasticCollision(fromCell, toCell, move);
                break;
            case CollisionType::FRAGMENTATION:
                handleFragmentation(fromCell, toCell, move);
                break;
            case CollisionType::ABSORPTION:
                handleAbsorption(fromCell, toCell, move);
                break;
        }
    }
    
    pending_moves_.clear();
    timers_.stopTimer("process_moves");
}

void WorldB::applyPressure(double /* deltaTime */)
{
    if (pressure_system_ == PressureSystem::Original) {
        calculateHydrostaticPressure();
    }
    // Other pressure systems ignored for now
}

void WorldB::calculateHydrostaticPressure()
{
    timers_.startTimer("hydrostatic_pressure");
    
    // Simple top-down hydrostatic pressure calculation
    for (uint32_t x = 0; x < width_; ++x) {
        double accumulatedPressure = 0.0;
        
        for (uint32_t y = 0; y < height_; ++y) {
            CellB& cell = at(x, y);
            
            cell.setPressure(accumulatedPressure * pressure_scale_);
            
            if (!cell.isEmpty()) {
                accumulatedPressure += cell.getEffectiveDensity() * gravity_;
            }
        }
    }
    
    timers_.stopTimer("hydrostatic_pressure");
}

void WorldB::setupBoundaryWalls()
{
    spdlog::info("Setting up boundary walls for WorldB");
    
    // Top and bottom walls
    for (uint32_t x = 0; x < width_; ++x) {
        at(x, 0).replaceMaterial(MaterialType::WALL, 1.0);
        at(x, height_ - 1).replaceMaterial(MaterialType::WALL, 1.0);
    }
    
    // Left and right walls
    for (uint32_t y = 0; y < height_; ++y) {
        at(0, y).replaceMaterial(MaterialType::WALL, 1.0);
        at(width_ - 1, y).replaceMaterial(MaterialType::WALL, 1.0);
    }
    
    spdlog::info("Boundary walls setup complete");
}

// =================================================================
// ELASTIC BOUNDARY REFLECTION SYSTEM
// =================================================================

void WorldB::applyBoundaryReflection(CellB& cell, const Vector2i& direction)
{
    Vector2d velocity = cell.getVelocity();
    Vector2d com = cell.getCOM();
    double elasticity = getMaterialProperties(cell.getMaterialType()).elasticity;
    
    spdlog::debug("Applying boundary reflection: material={} direction=({},{}) elasticity={:.2f} velocity=({:.2f},{:.2f})",
                  getMaterialName(cell.getMaterialType()), direction.x, direction.y, elasticity, velocity.x, velocity.y);
    
    // Apply elastic reflection for the component perpendicular to the boundary
    if (direction.x != 0) {  // Horizontal boundary (left/right walls)
        velocity.x = -velocity.x * elasticity;
        // Move COM away from boundary to prevent re-triggering boundary detection
        com.x = (direction.x > 0) ? 0.99 : -0.99;
    }
    
    if (direction.y != 0) {  // Vertical boundary (top/bottom walls)  
        velocity.y = -velocity.y * elasticity;
        // Move COM away from boundary to prevent re-triggering boundary detection
        com.y = (direction.y > 0) ? 0.99 : -0.99;
    }
    
    cell.setVelocity(velocity);
    cell.setCOM(com);
    
    spdlog::debug("Boundary reflection complete: new_velocity=({:.2f},{:.2f}) new_com=({:.2f},{:.2f})",
                  velocity.x, velocity.y, com.x, com.y);
}

void WorldB::applyCellBoundaryReflection(CellB& cell, const Vector2i& direction, MaterialType material)
{
    Vector2d velocity = cell.getVelocity();
    Vector2d com = cell.getCOM();
    double elasticity = getMaterialProperties(material).elasticity;
    
    spdlog::debug("Applying cell boundary reflection: material={} direction=({},{}) elasticity={:.2f}",
                  getMaterialName(material), direction.x, direction.y, elasticity);
    
    // Apply elastic reflection when transfer between cells fails
    if (direction.x != 0) {  // Horizontal transfer failed
        velocity.x = -velocity.x * elasticity;
        // Move COM away from the boundary that caused the failed transfer
        com.x = (direction.x > 0) ? 0.99 : -0.99;
    }
    
    if (direction.y != 0) {  // Vertical transfer failed
        velocity.y = -velocity.y * elasticity;
        // Move COM away from the boundary that caused the failed transfer  
        com.y = (direction.y > 0) ? 0.99 : -0.99;
    }
    
    cell.setVelocity(velocity);
    cell.setCOM(com);
    
    spdlog::debug("Cell boundary reflection complete: new_velocity=({:.2f},{:.2f}) new_com=({:.2f},{:.2f})",
                  velocity.x, velocity.y, com.x, com.y);
}

// =================================================================
// FLOATING PARTICLE COLLISION DETECTION
// =================================================================

bool WorldB::checkFloatingParticleCollision(int cellX, int cellY)
{
    if (!has_floating_particle_ || !isValidCell(cellX, cellY)) {
        return false;
    }
    
    const CellB& targetCell = at(cellX, cellY);
    
    // Check if there's material to collide with
    if (!targetCell.isEmpty()) {
        // Get material properties for collision behavior
        const MaterialProperties& floatingProps = getMaterialProperties(floating_particle_.getMaterialType());
        const MaterialProperties& targetProps = getMaterialProperties(targetCell.getMaterialType());
        
        // For now, simple collision detection - can be enhanced later
        // Heavy materials (like METAL) can push through lighter materials
        // Solid materials (like WALL) stop everything
        if (targetCell.getMaterialType() == MaterialType::WALL) {
            return true; // Wall stops everything
        }
        
        // Check density-based collision
        if (floatingProps.density <= targetProps.density) {
            return true; // Can't push through denser material
        }
    }
    
    return false;
}

void WorldB::handleFloatingParticleCollision(int cellX, int cellY)
{
    if (!has_floating_particle_ || !isValidCell(cellX, cellY)) {
        return;
    }
    
    CellB& targetCell = at(cellX, cellY);
    Vector2d particleVelocity = floating_particle_.getVelocity();
    
    spdlog::info("Floating particle {} collided with {} at cell ({},{}) with velocity ({:.2f},{:.2f})", 
                 getMaterialName(floating_particle_.getMaterialType()),
                 getMaterialName(targetCell.getMaterialType()),
                 cellX, cellY, particleVelocity.x, particleVelocity.y);
    
    // TODO: Implement collision response based on material properties
    // - Elastic collisions for METAL vs METAL
    // - Splash effects for WATER collisions
    // - Fragmentation for brittle materials
    // - Momentum transfer based on mass ratios
    
    // For now, simple momentum transfer
    Vector2d currentVelocity = targetCell.getVelocity();
    double floatingMass = floating_particle_.getMass();
    double targetMass = targetCell.getMass();
    
    if (targetMass > MIN_MATTER_THRESHOLD) {
        // Inelastic collision with momentum conservation
        Vector2d combinedMomentum = particleVelocity * floatingMass + currentVelocity * targetMass;
        Vector2d newVelocity = combinedMomentum / (floatingMass + targetMass);
        targetCell.setVelocity(newVelocity);
        targetCell.markDirty();
        
        spdlog::debug("Applied collision momentum: new velocity ({:.2f},{:.2f})", 
                      newVelocity.x, newVelocity.y);
    }
}

// =================================================================
// HELPER METHODS
// =================================================================

void WorldB::pixelToCell(int pixelX, int pixelY, int& cellX, int& cellY) const
{
    // Convert pixel coordinates to cell coordinates
    // Each cell is Cell::WIDTH x Cell::HEIGHT pixels
    cellX = pixelX / Cell::WIDTH;
    cellY = pixelY / Cell::HEIGHT;
}

bool WorldB::isValidCell(int x, int y) const
{
    return x >= 0 && y >= 0 && 
           static_cast<uint32_t>(x) < width_ && 
           static_cast<uint32_t>(y) < height_;
}

size_t WorldB::coordToIndex(uint32_t x, uint32_t y) const
{
    return y * width_ + x;
}

Vector2i WorldB::pixelToCell(int pixelX, int pixelY) const
{
    return Vector2i(pixelX / Cell::WIDTH, pixelY / Cell::HEIGHT);
}

bool WorldB::isValidCell(const Vector2i& pos) const
{
    return isValidCell(pos.x, pos.y);
}

size_t WorldB::coordToIndex(const Vector2i& pos) const
{
    return coordToIndex(static_cast<uint32_t>(pos.x), static_cast<uint32_t>(pos.y));
}

// =================================================================
// WORLD SETUP CONTROL METHODS
// =================================================================

void WorldB::setLeftThrowEnabled(bool enabled)
{
    ConfigurableWorldSetup* configSetup = dynamic_cast<ConfigurableWorldSetup*>(world_setup_.get());
    if (configSetup) {
        configSetup->setLeftThrowEnabled(enabled);
    }
    left_throw_enabled_ = enabled; // Keep member variable in sync
}

void WorldB::setRightThrowEnabled(bool enabled)
{
    ConfigurableWorldSetup* configSetup = dynamic_cast<ConfigurableWorldSetup*>(world_setup_.get());
    if (configSetup) {
        configSetup->setRightThrowEnabled(enabled);
    }
    right_throw_enabled_ = enabled; // Keep member variable in sync
}

void WorldB::setLowerRightQuadrantEnabled(bool enabled)
{
    ConfigurableWorldSetup* configSetup = dynamic_cast<ConfigurableWorldSetup*>(world_setup_.get());
    if (configSetup) {
        configSetup->setLowerRightQuadrantEnabled(enabled);
    }
    quadrant_enabled_ = enabled; // Keep member variable in sync
}

void WorldB::setWallsEnabled(bool enabled)
{
    ConfigurableWorldSetup* configSetup = dynamic_cast<ConfigurableWorldSetup*>(world_setup_.get());
    if (configSetup) {
        configSetup->setWallsEnabled(enabled);
    }
    walls_enabled_ = enabled; // Keep member variable in sync
    
    // Rebuild walls if needed
    if (enabled) {
        setupBoundaryWalls();
    } else {
        // Clear existing walls by resetting boundary cells to air
        for (uint32_t x = 0; x < width_; ++x) {
            at(x, 0).clear(); // Top wall
            at(x, height_ - 1).clear(); // Bottom wall
        }
        for (uint32_t y = 0; y < height_; ++y) {
            at(0, y).clear(); // Left wall
            at(width_ - 1, y).clear(); // Right wall
        }
    }
}

void WorldB::setRainRate(double rate)
{
    ConfigurableWorldSetup* configSetup = dynamic_cast<ConfigurableWorldSetup*>(world_setup_.get());
    if (configSetup) {
        configSetup->setRainRate(rate);
    }
    rain_rate_ = rate; // Keep member variable in sync
}

bool WorldB::isLeftThrowEnabled() const
{
    const ConfigurableWorldSetup* configSetup = dynamic_cast<const ConfigurableWorldSetup*>(world_setup_.get());
    return configSetup ? configSetup->isLeftThrowEnabled() : left_throw_enabled_;
}

bool WorldB::isRightThrowEnabled() const
{
    const ConfigurableWorldSetup* configSetup = dynamic_cast<const ConfigurableWorldSetup*>(world_setup_.get());
    return configSetup ? configSetup->isRightThrowEnabled() : right_throw_enabled_;
}

bool WorldB::isLowerRightQuadrantEnabled() const
{
    const ConfigurableWorldSetup* configSetup = dynamic_cast<const ConfigurableWorldSetup*>(world_setup_.get());
    return configSetup ? configSetup->isLowerRightQuadrantEnabled() : quadrant_enabled_;
}

bool WorldB::areWallsEnabled() const
{
    const ConfigurableWorldSetup* configSetup = dynamic_cast<const ConfigurableWorldSetup*>(world_setup_.get());
    return configSetup ? configSetup->areWallsEnabled() : walls_enabled_;
}

double WorldB::getRainRate() const
{
    const ConfigurableWorldSetup* configSetup = dynamic_cast<const ConfigurableWorldSetup*>(world_setup_.get());
    return configSetup ? configSetup->getRainRate() : rain_rate_;
}

// World type management implementation
WorldType WorldB::getWorldType() const
{
    return WorldType::RulesB;
}

void WorldB::preserveState(::WorldState& state) const
{
    // Initialize state with current world properties
    state.initializeGrid(width_, height_);
    state.timescale = timescale_;
    state.timestep = timestep_;
    
    // Copy physics parameters
    state.gravity = gravity_;
    state.elasticity_factor = elasticity_factor_;
    state.pressure_scale = pressure_scale_;
    state.dirt_fragmentation_factor = 1.0; // WorldB doesn't use fragmentation
    state.water_pressure_threshold = 0.0; // WorldB uses simplified pressure
    
    // Copy world setup flags
    state.left_throw_enabled = left_throw_enabled_;
    state.right_throw_enabled = right_throw_enabled_;
    state.lower_right_quadrant_enabled = quadrant_enabled_;
    state.walls_enabled = walls_enabled_;
    state.rain_rate = rain_rate_;
    
    // Copy time reversal state (WorldB doesn't support time reversal)
    state.time_reversal_enabled = false;
    
    // Copy control flags
    state.add_particles_enabled = add_particles_enabled_;
    state.cursor_force_enabled = cursor_force_enabled_;
    
    // Convert CellB data to WorldState::CellData
    for (uint32_t y = 0; y < height_; ++y) {
        for (uint32_t x = 0; x < width_; ++x) {
            const CellB& cell = at(x, y);
            
            // Calculate total mass based on fill ratio and material density
            double totalMass = 0.0;
            MaterialType cellMaterial = cell.getMaterialType();
            
            if (cellMaterial != MaterialType::AIR && cell.getFillRatio() > MIN_MATTER_THRESHOLD) {
                const MaterialProperties& props = getMaterialProperties(cellMaterial);
                totalMass = cell.getFillRatio() * props.density;
            }
            
            // Create CellData with CellB information
            ::WorldState::CellData cellData(
                totalMass,
                cellMaterial,
                cell.getVelocity(),
                cell.getCOM(),
                cell.getPressure()
            );
            
            state.setCellData(x, y, cellData);
        }
    }
    
    spdlog::info("WorldB state preserved: {}x{} grid with {} total mass", 
                width_, height_, getTotalMass());
}

void WorldB::restoreState(const ::WorldState& state)
{
    spdlog::info("Restoring WorldB state from {}x{} grid", state.width, state.height);
    
    // Resize grid if necessary
    if (state.width != width_ || state.height != height_) {
        resizeGrid(state.width, state.height, false);
    }
    
    // Restore physics parameters
    timescale_ = state.timescale;
    timestep_ = state.timestep;
    gravity_ = state.gravity;
    elasticity_factor_ = state.elasticity_factor;
    pressure_scale_ = state.pressure_scale;
    // Note: WorldB doesn't use dirt_fragmentation_factor or water_pressure_threshold
    
    // Restore world setup flags
    left_throw_enabled_ = state.left_throw_enabled;
    right_throw_enabled_ = state.right_throw_enabled;
    quadrant_enabled_ = state.lower_right_quadrant_enabled;
    walls_enabled_ = state.walls_enabled;
    rain_rate_ = state.rain_rate;
    
    // Restore control flags
    add_particles_enabled_ = state.add_particles_enabled;
    cursor_force_enabled_ = state.cursor_force_enabled;
    
    // Convert WorldState::CellData back to CellB data
    for (uint32_t y = 0; y < height_; ++y) {
        for (uint32_t x = 0; x < width_; ++x) {
            const ::WorldState::CellData& cellData = state.getCellData(x, y);
            CellB& cell = at(x, y);
            
            // Convert from mixed material data to pure CellB format
            if (cellData.material_mass > MIN_MATTER_THRESHOLD && 
                cellData.dominant_material != MaterialType::AIR) {
                
                // Calculate fill ratio from mass and material density
                const MaterialProperties& props = getMaterialProperties(cellData.dominant_material);
                double fillRatio = props.density > 0.0 ? 
                                  std::min(1.0, cellData.material_mass / props.density) : 
                                  cellData.material_mass;
                
                // Update cell with pure material
                cell.setMaterialType(cellData.dominant_material);
                cell.setFillRatio(fillRatio);
                cell.setVelocity(cellData.velocity);
                cell.setCOM(cellData.com);
                cell.setPressure(cellData.pressure);
            } else {
                // Empty cell
                cell.clear();
            }
            
            cell.markDirty();
        }
    }
    
    spdlog::info("WorldB state restored: {} total mass", getTotalMass());
}

// =================================================================
// ENHANCED COLLISION DETECTION AND PHYSICS
// =================================================================

std::vector<Vector2i> WorldB::getAllBoundaryCrossings(const Vector2d& newCOM)
{
    std::vector<Vector2i> crossings;
    
    // Check each boundary independently (aligned with original shouldTransfer logic)
    if (newCOM.x >= 1.0) crossings.push_back(Vector2i(1, 0));   // Right boundary
    if (newCOM.x <= -1.0) crossings.push_back(Vector2i(-1, 0)); // Left boundary
    if (newCOM.y >= 1.0) crossings.push_back(Vector2i(0, 1));   // Down boundary  
    if (newCOM.y <= -1.0) crossings.push_back(Vector2i(0, -1)); // Up boundary
    
    return crossings;
}

WorldB::MaterialMove WorldB::createCollisionAwareMove(const CellB& fromCell, const CellB& toCell, 
                                                      const Vector2i& fromPos, const Vector2i& toPos,
                                                      const Vector2i& direction, double /* deltaTime */)
{
    MaterialMove move;
    
    // Standard move data
    move.fromX = fromPos.x;
    move.fromY = fromPos.y;
    move.toX = toPos.x;
    move.toY = toPos.y;
    move.material = fromCell.getMaterialType();
    move.amount = std::min(fromCell.getFillRatio(), 1.0 - toCell.getFillRatio());
    move.momentum = fromCell.getVelocity();
    move.boundary_normal = Vector2d(direction.x, direction.y);
    
    // NEW: Calculate collision physics data
    move.material_mass = calculateMaterialMass(fromCell);
    move.target_mass = calculateMaterialMass(toCell);
    move.collision_energy = calculateCollisionEnergy(move, fromCell, toCell);
    
    // Determine collision type based on materials and energy
    move.collision_type = determineCollisionType(fromCell.getMaterialType(), 
                                                 toCell.getMaterialType(), 
                                                 move.collision_energy);
    
    // Set material-specific restitution coefficient
    const auto& fromProps = getMaterialProperties(fromCell.getMaterialType());
    const auto& toProps = getMaterialProperties(toCell.getMaterialType());
    
    if (move.collision_type == CollisionType::ELASTIC_REFLECTION) {
        // For elastic collisions, use geometric mean of elasticities
        move.restitution_coefficient = std::sqrt(fromProps.elasticity * toProps.elasticity);
    } else if (move.collision_type == CollisionType::INELASTIC_COLLISION) {
        // For inelastic collisions, reduce restitution significantly
        move.restitution_coefficient = std::sqrt(fromProps.elasticity * toProps.elasticity) * 0.3;
    } else if (move.collision_type == CollisionType::FRAGMENTATION) {
        // Fragmentation has very low restitution
        move.restitution_coefficient = 0.1;
    } else {
        // Transfer and absorption have minimal bounce
        move.restitution_coefficient = 0.0;
    }
    
    return move;
}

WorldB::CollisionType WorldB::determineCollisionType(MaterialType from, MaterialType to, double collision_energy)
{
    // Get material properties for both materials
    const auto& fromProps = getMaterialProperties(from);
    const auto& toProps = getMaterialProperties(to);
    
    // Empty cells allow transfer
    if (to == MaterialType::AIR) {
        return CollisionType::TRANSFER_ONLY;
    }
    
    // High-energy impacts on brittle materials cause fragmentation
    const double FRAGMENTATION_THRESHOLD = 15.0;
    if (collision_energy > FRAGMENTATION_THRESHOLD && 
        (from == MaterialType::WOOD || from == MaterialType::LEAF) &&
        (to == MaterialType::METAL || to == MaterialType::WALL)) {
        return CollisionType::FRAGMENTATION;
    }
    
    // Material-specific interaction matrix
    
    // METAL interactions - highly elastic due to high elasticity (0.8)
    if (from == MaterialType::METAL || to == MaterialType::METAL) {
        if (to == MaterialType::WALL || from == MaterialType::WALL) {
            return CollisionType::ELASTIC_REFLECTION; // Metal vs wall
        }
        if ((from == MaterialType::METAL && to == MaterialType::METAL) ||
            (from == MaterialType::METAL && isMaterialRigid(to)) ||
            (to == MaterialType::METAL && isMaterialRigid(from))) {
            return CollisionType::ELASTIC_REFLECTION; // Metal vs rigid materials
        }
        return CollisionType::INELASTIC_COLLISION; // Metal vs soft materials
    }
    
    // WALL interactions - always elastic due to infinite mass
    if (to == MaterialType::WALL) {
        return CollisionType::ELASTIC_REFLECTION;
    }
    
    // WOOD interactions - moderately elastic (0.6 elasticity)
    if (from == MaterialType::WOOD && isMaterialRigid(to)) {
        return CollisionType::ELASTIC_REFLECTION;
    }
    
    // AIR interactions - highly elastic (1.0 elasticity) but low mass
    if (from == MaterialType::AIR) {
        return CollisionType::ELASTIC_REFLECTION;
    }
    
    // Rigid-to-rigid collisions based on elasticity
    if (isMaterialRigid(from) && isMaterialRigid(to)) {
        double avg_elasticity = (fromProps.elasticity + toProps.elasticity) / 2.0;
        return (avg_elasticity > 0.5) ? CollisionType::ELASTIC_REFLECTION : CollisionType::INELASTIC_COLLISION;
    }
    
    // Fluid absorption behaviors
    if ((from == MaterialType::WATER && to == MaterialType::DIRT) ||
        (from == MaterialType::DIRT && to == MaterialType::WATER)) {
        return CollisionType::ABSORPTION;
    }
    
    // Dense materials hitting lighter materials
    if (fromProps.density > toProps.density * 2.0) {
        return CollisionType::INELASTIC_COLLISION; // Heavy impacts soft
    }
    
    // Default: inelastic collision for general material interactions
    return CollisionType::INELASTIC_COLLISION;
}

double WorldB::calculateMaterialMass(const CellB& cell)
{
    if (cell.isEmpty()) return 0.0;
    
    // Mass = density × volume
    // Volume = fill_ratio (since cell volume is normalized to 1.0)
    double density = getMaterialDensity(cell.getMaterialType());
    double volume = cell.getFillRatio();
    return density * volume;
}

double WorldB::calculateCollisionEnergy(const MaterialMove& move, const CellB& fromCell, const CellB& toCell)
{
    // Kinetic energy: KE = 0.5 × m × v²
    double movingMass = calculateMaterialMass(fromCell) * move.amount;
    double velocity_magnitude = move.momentum.length();
    
    // If target cell has material, include reduced mass for collision
    double targetMass = calculateMaterialMass(toCell);
    double effective_mass = movingMass;
    
    if (targetMass > 0.0) {
        // Reduced mass formula: μ = (m1 × m2) / (m1 + m2)
        effective_mass = (movingMass * targetMass) / (movingMass + targetMass);
    }
    
    return 0.5 * effective_mass * velocity_magnitude * velocity_magnitude;
}

// =================================================================
// COLLISION HANDLERS
// =================================================================

void WorldB::handleTransferMove(CellB& fromCell, CellB& toCell, const MaterialMove& move)
{
    // Attempt the transfer
    const double transferred = fromCell.transferToWithPhysics(toCell, move.amount, move.boundary_normal);
    
    if (transferred > 0.0) {
        spdlog::trace("Transferred {:.3f} {} from ({},{}) to ({},{}) with boundary normal ({:.2f},{:.2f})", 
                      transferred, getMaterialName(move.material),
                      move.fromX, move.fromY, move.toX, move.toY,
                      move.boundary_normal.x, move.boundary_normal.y);
    }
    
    // Check if transfer was incomplete (target full or couldn't accept all material)
    const double transfer_deficit = move.amount - transferred;
    if (transfer_deficit > MIN_MATTER_THRESHOLD) {
        // Transfer failed partially or completely - apply elastic reflection for remaining material
        Vector2i direction(move.toX - move.fromX, move.toY - move.fromY);
        
        spdlog::debug("Transfer incomplete: requested={:.3f}, transferred={:.3f}, deficit={:.3f} - applying reflection",
                      move.amount, transferred, transfer_deficit);
        
        applyCellBoundaryReflection(fromCell, direction, move.material);
    }
}

void WorldB::handleElasticCollision(CellB& fromCell, CellB& toCell, const MaterialMove& move)
{
    Vector2d incident_velocity = move.momentum;
    Vector2d surface_normal = move.boundary_normal.normalize();
    
    if (move.target_mass > 0.0 && !toCell.isEmpty()) {
        // Proper elastic collision formula for two-body collision
        Vector2d target_velocity = toCell.getVelocity();
        double m1 = move.material_mass;
        double m2 = move.target_mass;
        Vector2d v1 = incident_velocity;
        Vector2d v2 = target_velocity;
        
        // Elastic collision formulas: v1' = ((m1-m2)v1 + 2m2v2)/(m1+m2)
        //                            v2' = ((m2-m1)v2 + 2m1v1)/(m1+m2)
        Vector2d new_v1 = ((m1 - m2) * v1 + 2.0 * m2 * v2) / (m1 + m2);
        Vector2d new_v2 = ((m2 - m1) * v2 + 2.0 * m1 * v1) / (m1 + m2);
        
        // Apply restitution coefficient for energy loss
        fromCell.setVelocity(new_v1 * move.restitution_coefficient);
        toCell.setVelocity(new_v2 * move.restitution_coefficient);
        
        spdlog::trace("Elastic collision: {} vs {} at ({},{}) -> ({},{}) - masses: {:.2f}, {:.2f}, restitution: {:.2f}", 
                      getMaterialName(move.material), getMaterialName(toCell.getMaterialType()),
                      move.fromX, move.fromY, move.toX, move.toY, m1, m2, move.restitution_coefficient);
    } else {
        // Empty target or zero mass - just reflect off surface
        Vector2d reflected_velocity = incident_velocity.reflect(surface_normal) * move.restitution_coefficient;
        fromCell.setVelocity(reflected_velocity);
        
        spdlog::trace("Elastic reflection: {} bounced off surface at ({},{}) with restitution {:.2f}", 
                      getMaterialName(move.material), move.fromX, move.fromY, move.restitution_coefficient);
    }
    
    // Minimal or no material transfer for elastic collisions
    // Material stays in original cell with new velocity
}

void WorldB::handleInelasticCollision(CellB& fromCell, CellB& toCell, const MaterialMove& move)
{
    // Similar to elastic but with energy loss and some material transfer
    Vector2d incident_velocity = move.momentum;
    Vector2d surface_normal = move.boundary_normal.normalize();
    
    // Reduced restitution for inelastic collision
    double inelastic_restitution = move.restitution_coefficient * 0.5;
    Vector2d reflected_velocity = incident_velocity.reflect(surface_normal) * inelastic_restitution;
    
    // Apply momentum conservation with energy loss
    fromCell.setVelocity(reflected_velocity);
    
    // Allow some material transfer (reduced amount)
    double reduced_amount = move.amount * 0.3; // Transfer 30% of material
    MaterialMove transfer_move = move;
    transfer_move.amount = reduced_amount;
    handleTransferMove(fromCell, toCell, transfer_move);
    
    spdlog::trace("Inelastic collision: {} at ({},{}) with partial transfer {:.3f}", 
                  getMaterialName(move.material), move.fromX, move.fromY, reduced_amount);
}

void WorldB::handleFragmentation(CellB& fromCell, CellB& toCell, const MaterialMove& move)
{
    // TODO: Implement fragmentation mechanics
    // For now, treat as inelastic collision with complete material transfer
    spdlog::debug("Fragmentation collision: {} at ({},{}) - treating as inelastic for now", 
                  getMaterialName(move.material), move.fromX, move.fromY);
    
    handleInelasticCollision(fromCell, toCell, move);
}

void WorldB::handleAbsorption(CellB& fromCell, CellB& toCell, const MaterialMove& move)
{
    // One material absorbs the other - implement absorption logic
    if (move.material == MaterialType::WATER && toCell.getMaterialType() == MaterialType::DIRT) {
        // Water absorbed by dirt - transfer all water
        handleTransferMove(fromCell, toCell, move);
        spdlog::trace("Absorption: WATER absorbed by DIRT at ({},{})", move.toX, move.toY);
    } else if (move.material == MaterialType::DIRT && toCell.getMaterialType() == MaterialType::WATER) {
        // Dirt falls into water - mix materials
        handleTransferMove(fromCell, toCell, move);
        spdlog::trace("Absorption: DIRT mixed with WATER at ({},{})", move.toX, move.toY);
    } else {
        // Default to regular transfer
        handleTransferMove(fromCell, toCell, move);
    }
}
