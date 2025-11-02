#include "SimulationManager.h"
#include "EventRouter.h"
#include "SimulatorUI.h"
#include "SparkleAssert.h"
#include "World.h"
#include "WorldEventGenerator.h"
#include "scenarios/Scenario.h"
#include "scenarios/ScenarioRegistry.h"
#include "spdlog/spdlog.h"

SimulationManager::SimulationManager(
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
      default_height_(height)
{
    spdlog::info("Creating SimulationManager with {}x{} grid", width, height);

    // Create the world.
    world_ = std::make_unique<World>(width, height);
    if (!world_) {
        throw std::runtime_error("Failed to create world");
    }
    world_->setWallsEnabled(false); // Default to walls disabled.
    spdlog::info("Created World physics system");

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

SimulationManager::~SimulationManager() = default;

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
        auto setup = sandboxScenario->createWorldEventGenerator();
        world_->setWorldEventGenerator(std::move(setup));
    }
    else {
        spdlog::warn("Sandbox scenario not found in registry - using default world setup");
        // World will use its default setup from the constructor
    }

    spdlog::info("SimulationManager initialization complete");
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
        "Resizing world from {}x{} to {}x{}", width_, height_, requiredWidth, requiredHeight);

    // Update dimensions.
    width_ = requiredWidth;
    height_ = requiredHeight;

    // Create new world with new dimensions.
    world_ = std::make_unique<World>(width_, height_);
    if (!world_) {
        spdlog::error("Failed to create resized world");
        return false;
    }

    // Reconnect UI if it exists
    if (ui_) {
        connectUIAndWorld();
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

void SimulationManager::connectUIAndWorld()
{
    if (!ui_ || !world_) {
        return;
    }

    spdlog::info("Connecting UI and world");

    // Set up relationship.
    ui_->setWorld(world_.get());
    ui_->setSimulationManager(this);

    spdlog::info("UI and world connected");

    // Populate UI controls with values from the world.
    ui_->populateFromWorld();
}
