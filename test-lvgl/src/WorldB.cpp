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
      ui_ref_(nullptr),
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
    spdlog::info("After wall setup, about to do WorldSetup");
    
    // Use the world setup strategy to initialize the world
    spdlog::info("About to check world_setup_ pointer...");
    if (world_setup_) {
        spdlog::info("Calling WorldSetup::setup for WorldB");
        world_setup_->setup(*this);
    } else {
        spdlog::error("WorldSetup is null in WorldB::reset()!");
    }
    
    spdlog::info("WorldB reset complete");
    spdlog::info("DEBUGGING: Total mass after reset = {:.3f}", getTotalMass());
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
        CellB& cell = at(static_cast<uint32_t>(cellX), static_cast<uint32_t>(cellY));
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
                Vector2i transferOffset(static_cast<int>(transferDir.x), static_cast<int>(transferDir.y));
                Vector2i currentPos(x, y);
                Vector2i targetPos = currentPos + transferOffset;
                
                if (isValidCell(targetPos.x, targetPos.y)) {
                    // Queue a material move
                    MaterialMove move;
                    move.fromX = x;
                    move.fromY = y;
                    move.toX = targetPos.x;
                    move.toY = targetPos.y;
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
