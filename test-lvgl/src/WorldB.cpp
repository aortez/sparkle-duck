#include "WorldB.h"
#include "SimulatorUI.h"
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
      dragged_amount_(0.0)
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
    
    const double scaledDeltaTime = deltaTimeSeconds * timescale_;
    
    if (scaledDeltaTime > 0.0) {
        // Main physics steps
        applyGravity(scaledDeltaTime);
        processVelocityLimiting();
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
    timers_.startTimer("draw");
    
    // Simple debug rendering for now
    // TODO: Implement proper LVGL-based rendering
    spdlog::trace("WorldB::draw() - rendering {} cells", cells_.size());
    
    timers_.stopTimer("draw");
}

void WorldB::reset()
{
    spdlog::info("Resetting WorldB to initial state");
    
    timestep_ = 0;
    removed_mass_ = 0.0;
    pending_moves_.clear();
    
    // Clear all cells to air
    for (auto& cell : cells_) {
        cell.clear();
    }
    
    // Rebuild boundary walls
    if (walls_enabled_) {
        setupBoundaryWalls();
    }
    
    spdlog::info("WorldB reset complete");
}

// =================================================================
// MATERIAL ADDITION METHODS
// =================================================================

void WorldB::addDirtAtPixel(int pixelX, int pixelY)
{
    int cellX, cellY;
    pixelToCell(pixelX, pixelY, cellX, cellY);
    
    if (isValidCell(cellX, cellY)) {
        addMaterialAtCell(cellX, cellY, MaterialType::DIRT, 1.0);
        spdlog::debug("Added DIRT at pixel ({},{}) -> cell ({},{})", 
                      pixelX, pixelY, cellX, cellY);
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
        
        // Remove material from source cell
        cell.clear();
        
        spdlog::debug("Started dragging {} from cell ({},{})", 
                      getMaterialName(dragged_material_), cellX, cellY);
    }
}

void WorldB::updateDrag(int pixelX, int pixelY)
{
    // For now, just track position - no continuous preview
    spdlog::trace("Drag update to pixel ({},{})", pixelX, pixelY);
}

void WorldB::endDragging(int pixelX, int pixelY)
{
    if (!is_dragging_) {
        return;
    }
    
    int cellX, cellY;
    pixelToCell(pixelX, pixelY, cellX, cellY);
    
    if (isValidCell(cellX, cellY)) {
        addMaterialAtCell(cellX, cellY, dragged_material_, dragged_amount_);
        spdlog::debug("Ended drag: placed {} at cell ({},{})", 
                      getMaterialName(dragged_material_), cellX, cellY);
    }
    
    // Reset drag state
    is_dragging_ = false;
    drag_start_x_ = -1;
    drag_start_y_ = -1;
    dragged_material_ = MaterialType::AIR;
    dragged_amount_ = 0.0;
}

void WorldB::restoreLastDragCell()
{
    if (is_dragging_ && isValidCell(drag_start_x_, drag_start_y_)) {
        addMaterialAtCell(drag_start_x_, drag_start_y_, dragged_material_, dragged_amount_);
        spdlog::debug("Restored dragged material to origin ({},{})", drag_start_x_, drag_start_y_);
    }
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

void WorldB::resizeGrid(uint32_t newWidth, uint32_t newHeight, bool clearHistory)
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

double WorldB::getTotalMass() const
{
    double totalMass = 0.0;
    
    for (const auto& cell : cells_) {
        totalMass += cell.getMass();
    }
    
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

void WorldB::processVelocityLimiting()
{
    for (auto& cell : cells_) {
        if (!cell.isEmpty()) {
            cell.limitVelocity(MAX_VELOCITY, VELOCITY_DAMPING_THRESHOLD, VELOCITY_DAMPING_FACTOR);
        }
    }
}

void WorldB::updateTransfers(double deltaTime)
{
    timers_.startTimer("update_transfers");
    
    // Clear previous moves
    pending_moves_.clear();
    
    // Queue material moves based on COM positions and velocities
    queueMaterialMoves();
    
    timers_.stopTimer("update_transfers");
}

void WorldB::queueMaterialMoves()
{
    for (uint32_t y = 0; y < height_; ++y) {
        for (uint32_t x = 0; x < width_; ++x) {
            CellB& cell = at(x, y);
            
            if (cell.isEmpty() || cell.isWall()) {
                continue;
            }
            
            // Update COM based on velocity
            Vector2d newCOM = cell.getCOM() + cell.getVelocity();
            cell.setCOM(newCOM);
            
            // Check if material should transfer to neighboring cell
            if (cell.shouldTransfer()) {
                Vector2d transferDir = cell.getTransferDirection();
                
                const int targetX = static_cast<int>(x) + static_cast<int>(transferDir.x);
                const int targetY = static_cast<int>(y) + static_cast<int>(transferDir.y);
                
                if (isValidCell(targetX, targetY)) {
                    // Queue a material move
                    MaterialMove move;
                    move.fromX = x;
                    move.fromY = y;
                    move.toX = targetX;
                    move.toY = targetY;
                    move.amount = cell.getFillRatio() * 0.5; // Transfer half the material
                    move.material = cell.getMaterialType();
                    move.momentum = cell.getVelocity() * cell.getMass();
                    
                    pending_moves_.push_back(move);
                }
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
        
        // Attempt the transfer
        const double transferred = fromCell.transferTo(toCell, move.amount);
        
        if (transferred > 0.0) {
            // Apply momentum conservation: new_COM = (m1*COM1 + m2*COM2)/(m1+m2)
            const double fromMass = fromCell.getMass();
            const double toMass = toCell.getMass();
            
            if (toMass > MIN_MATTER_THRESHOLD) {
                const Vector2d combinedMomentum = 
                    fromCell.getVelocity() * fromMass + move.momentum;
                const Vector2d newVelocity = combinedMomentum / toMass;
                toCell.setVelocity(newVelocity);
            }
            
            spdlog::trace("Transferred {:.3f} {} from ({},{}) to ({},{})", 
                          transferred, getMaterialName(move.material),
                          move.fromX, move.fromY, move.toX, move.toY);
        }
    }
    
    pending_moves_.clear();
    timers_.stopTimer("process_moves");
}

void WorldB::applyPressure(double deltaTime)
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
// HELPER METHODS
// =================================================================

void WorldB::pixelToCell(int pixelX, int pixelY, int& cellX, int& cellY) const
{
    // Simple mapping - assuming 1:1 pixel to cell ratio for now
    // TODO: Implement proper pixel-to-cell conversion based on Cell::WIDTH/HEIGHT
    cellX = pixelX;
    cellY = pixelY;
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