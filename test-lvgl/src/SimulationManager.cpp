#include "SimulationManager.h"
#include "EventRouter.h"
#include "SimulatorUI.h"
#include "SparkleAssert.h"
#include "WorldSetup.h"
#include "scenarios/Scenario.h"
#include "scenarios/ScenarioRegistry.h"
#include "spdlog/spdlog.h"

SimulationManager::SimulationManager(
    WorldType initialType,
    uint32_t width,
    uint32_t height,
    lv_obj_t* screen,
    EventRouter* eventRouter)
    : world_(nullptr),
      ui_(nullptr),
      draw_area_(nullptr),
      eventRouter_(eventRouter),
      width_(width),
      height_(height),
      default_width_(width),
      default_height_(height),
      initial_world_type_(initialType)
{
    spdlog::info("Creating SimulationManager with {}x{} grid", width, height);

    // Create the world first (without draw_area).
    world_ = createWorld(initial_world_type_);
    if (!world_) {
        throw std::runtime_error("Failed to create initial world");
    }
    spdlog::info("Created {} physics system", getWorldTypeName(initial_world_type_));

    // Create UI if screen is provided.
    if (screen) {
        ui_ = std::make_unique<SimulatorUI>(screen, eventRouter);
        spdlog::info("SimulationManager created with UI and EventRouter");
    }
    else {
        spdlog::info("SimulationManager created in headless mode");
    }

    spdlog::info("SimulationManager construction complete");
}

void SimulationManager::initialize()
{
    spdlog::info("Initializing SimulationManager");

    // Initialize UI first if it exists.
    if (ui_) {
        ui_->initialize();
        draw_area_ = ui_->getDrawArea();
        spdlog::info("UI initialized, draw_area obtained");
    }

    // Connect UI and world if UI exists.
    if (ui_) {
        connectUIAndWorld();
    }

    // Apply the default Sandbox scenario if available
    auto& registry = ScenarioRegistry::getInstance();
    auto* sandboxScenario = registry.getScenario("sandbox");

    if (sandboxScenario) {
        spdlog::info("Applying default Sandbox scenario");
        auto setup = sandboxScenario->createWorldSetup();
        world_->setWorldSetup(std::move(setup));
    }
    else {
        spdlog::warn("Sandbox scenario not found in registry - using default world setup");
        // World will use its default setup from the constructor
    }

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

    spdlog::info(
        "Switching from {} to {}", getWorldTypeName(currentType), getWorldTypeName(newType));

    // Step 1: Preserve current world state.
    WorldState state;
    world_->preserveState(state);
    spdlog::info("State preserved - grid: {}x{}, mass: {:.2f}", state.width, state.height, [&]() {
        double total = 0.0;
        for (const auto& row : state.grid_data) {
            for (const auto& cell : row) {
                total += cell.material_mass;
            }
        }
        return total;
    }());

    // Step 2: Create new world.
    auto newWorld = createWorld(newType);
    if (!newWorld) {
        spdlog::error("Failed to create new world of type {}", getWorldTypeName(newType));
        return false;
    }

    // Step 3: Restore state to new world.
    newWorld->restoreState(state);
    spdlog::info("State restored to new world");

    // Step 4: Replace current world (this automatically cleans up old world).
    world_ = std::move(newWorld);

    // Step 5: Reconnect UI to new world.
    if (ui_) {
        connectUIAndWorld();
        updateUIWorldType();
    }

    spdlog::info("World type switch completed successfully to {}", getWorldTypeName(newType));
    return true;
}

bool SimulationManager::resizeWorldIfNeeded(uint32_t requiredWidth, uint32_t requiredHeight)
{
    // If no specific dimensions required, restore default dimensions
    if (requiredWidth == 0 || requiredHeight == 0) {
        requiredWidth = default_width_;
        requiredHeight = default_height_;
    }

    // Check if current dimensions match
    if (width_ == requiredWidth && height_ == requiredHeight) {
        return false;
    }

    spdlog::info(
        "Resizing world from {}x{} to {}x{} for scenario",
        width_,
        height_,
        requiredWidth,
        requiredHeight);

    // Preserve current world type and any necessary state
    WorldType currentType = world_ ? world_->getWorldType() : initial_world_type_;

    // Update dimensions
    width_ = requiredWidth;
    height_ = requiredHeight;

    // Create new world with new dimensions
    world_ = createWorld(currentType);
    if (!world_) {
        spdlog::error("Failed to create resized world");
        return false;
    }

    // Reconnect UI if it exists
    if (ui_) {
        connectUIAndWorld();
        updateUIWorldType();
    }

    spdlog::info("World resized successfully to {}x{}", requiredWidth, requiredHeight);
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
    if (world_ && draw_area_) {
        world_->draw(*draw_area_);
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

bool SimulationManager::shouldExit() const
{
    if (!eventRouter_) {
        throw std::runtime_error("SimulationManager::shouldExit() called without EventRouter");
    }
    return eventRouter_->getSharedSimState().getShouldExit();
}

std::unique_ptr<WorldInterface> SimulationManager::createWorld(WorldType type)
{
    spdlog::info("Creating world of type {}", getWorldTypeName(type));
    return ::createWorld(type, width_, height_); // No draw_area parameter needed anymore.
}

void SimulationManager::connectUIAndWorld()
{
    if (!ui_ || !world_) {
        return;
    }

    spdlog::info("Connecting UI and world");

    // Set up bidirectional relationship.
    ui_->setWorld(world_.get());
    ui_->setSimulationManager(this);

    // Important: The world needs a reference to the UI for mass/FPS updates
    // but in our architecture, the SimulationManager owns both, so the world
    // gets a raw pointer reference (not ownership).
    world_->setUIReference(ui_.get());

    spdlog::info("UI and world connected");

    // Populate UI controls with values from the world.
    ui_->populateFromWorld();
}

void SimulationManager::updateUIWorldType()
{
    if (!ui_ || !world_) {
        return;
    }

    // This would update UI elements like button matrix to reflect current world type.
    // For now, we'll rely on the UI's internal update mechanisms.
    spdlog::debug("UI world type updated");
}
