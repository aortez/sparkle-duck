#include "SimulationManager.h"
#include "SimulatorUI.h"
#include "WorldSetup.h"
#include "spdlog/spdlog.h"

SimulationManager::SimulationManager(WorldType initialType, uint32_t width, uint32_t height, lv_obj_t* screen)
    : world_(nullptr), ui_(nullptr), draw_area_(nullptr), width_(width), height_(height), initial_world_type_(initialType)
{
    spdlog::info("Creating SimulationManager with {}x{} grid", width, height);
    
    // Create UI first if screen is provided, but don't get draw_area yet
    if (screen) {
        ui_ = std::make_unique<SimulatorUI>(screen);
        spdlog::info("SimulationManager created with UI");
    } else {
        spdlog::info("SimulationManager created in headless mode");
    }
    
    // Note: We'll create the world during initialize() after UI is set up
    spdlog::info("SimulationManager construction complete");
}

void SimulationManager::initialize()
{
    spdlog::info("Initializing SimulationManager");
    
    // Initialize UI first if it exists
    if (ui_) {
        ui_->initialize();
        draw_area_ = ui_->getDrawArea();
        spdlog::info("UI initialized, draw_area obtained");
    }
    
    // Now create the world with the proper draw_area
    world_ = createWorld(initial_world_type_);
    if (!world_) {
        throw std::runtime_error("Failed to create initial world");
    }
    
    // Connect UI and world if UI exists
    if (ui_) {
        connectUIAndWorld();
    }
    
    // Setup world with initial materials
    world_->setup();
    
    spdlog::info("SimulationManager initialization complete");
}

bool SimulationManager::switchWorldType(WorldType newType)
{
    if (!world_) {
        spdlog::error("Cannot switch world type - no world exists");
        return false;
    }
    
    WorldType currentType = world_->getWorldType();
    if (currentType == newType) {
        spdlog::info("Already using {} - no switch needed", getWorldTypeName(newType));
        return true;
    }
    
    spdlog::info("Switching from {} to {}", getWorldTypeName(currentType), getWorldTypeName(newType));
    
    // Step 1: Preserve current world state
    WorldState state;
    world_->preserveState(state);
    spdlog::info("State preserved - grid: {}x{}, mass: {:.2f}", 
                 state.width, state.height, 
                 [&]() {
                     double total = 0.0;
                     for (const auto& row : state.grid_data) {
                         for (const auto& cell : row) {
                             total += cell.material_mass;
                         }
                     }
                     return total;
                 }());
    
    // Step 2: Create new world
    auto newWorld = createWorld(newType);
    if (!newWorld) {
        spdlog::error("Failed to create new world of type {}", getWorldTypeName(newType));
        return false;
    }
    
    // Step 3: Restore state to new world
    newWorld->restoreState(state);
    spdlog::info("State restored to new world");
    
    // Step 4: Replace current world (this automatically cleans up old world)
    world_ = std::move(newWorld);
    
    // Step 5: Reconnect UI to new world
    if (ui_) {
        connectUIAndWorld();
        updateUIWorldType();
    }
    
    spdlog::info("World type switch completed successfully to {}", getWorldTypeName(newType));
    return true;
}

void SimulationManager::reset()
{
    if (world_) {
        spdlog::info("SimulationManager resetting world");
        world_->setup();
    }
}

void SimulationManager::advanceTime(double deltaTime)
{
    if (world_) {
        world_->advanceTime(deltaTime);
    }
}

void SimulationManager::draw()
{
    if (world_) {
        world_->draw();
    }
}

WorldType SimulationManager::getCurrentWorldType() const
{
    return world_ ? world_->getWorldType() : WorldType::RulesB;
}

void SimulationManager::preserveState(WorldState& state) const
{
    if (world_) {
        world_->preserveState(state);
    }
}

void SimulationManager::restoreState(const WorldState& state)
{
    if (world_) {
        world_->restoreState(state);
    }
}

void SimulationManager::dumpTimerStats() const
{
    if (world_) {
        world_->dumpTimerStats();
    }
}

std::unique_ptr<WorldInterface> SimulationManager::createWorld(WorldType type)
{
    spdlog::info("Creating world of type {}", getWorldTypeName(type));
    return ::createWorld(type, width_, height_, draw_area_);
}

void SimulationManager::connectUIAndWorld()
{
    if (!ui_ || !world_) {
        return;
    }
    
    spdlog::info("Connecting UI and world");
    
    // Set up bidirectional relationship
    ui_->setWorld(world_.get());
    ui_->setSimulationManager(this);
    
    // Important: The world needs a reference to the UI for mass/FPS updates
    // but in our architecture, the SimulationManager owns both, so the world
    // gets a raw pointer reference (not ownership)
    world_->setUIReference(ui_.get());
    
    spdlog::info("UI and world connected");
}

void SimulationManager::updateUIWorldType()
{
    if (!ui_ || !world_) {
        return;
    }
    
    // This would update UI elements like button matrix to reflect current world type
    // For now, we'll rely on the UI's internal update mechanisms
    spdlog::debug("UI world type updated");
}