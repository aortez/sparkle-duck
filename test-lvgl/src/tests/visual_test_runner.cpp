#include "visual_test_runner.h"
#include <future>
#include <spdlog/spdlog.h>
#include "../WorldB.h"
#include "../WorldInterface.h"

// External global settings used by the backend system
extern simulator_settings_t settings;

// VisualTestEnvironment static member definitions
bool VisualTestEnvironment::debug_logging_enabled_ = true;
bool VisualTestEnvironment::adhesion_disabled_by_default_ = true;
bool VisualTestEnvironment::cohesion_disabled_by_default_ = true;
bool VisualTestEnvironment::pressure_disabled_by_default_ = true;
bool VisualTestEnvironment::ascii_logging_enabled_ = true;

// VisualTestCoordinator implementation
VisualTestCoordinator& VisualTestCoordinator::getInstance() {
    static VisualTestCoordinator instance;
    return instance;
}

bool VisualTestCoordinator::initializeVisualMode() {
    if (visual_initialized_) return true;

    const char* visual_mode = std::getenv("SPARKLE_DUCK_VISUAL_TESTS");
    if (!visual_mode || (std::string(visual_mode) != "1" && std::string(visual_mode) != "true")) {
        visual_mode_enabled_ = false;
        return false;
    }

    std::cout << "\n=== Initializing Visual Test Mode ===\n";
    lv_init();
    settings.window_width = 800;
    settings.window_height = 600;
    settings.max_steps = 0;
    driver_backends_register();
    if (driver_backends_init_backend("wayland") == -1) {
        std::cout << "Failed to initialize Wayland backend - visual mode disabled\n";
        visual_mode_enabled_ = false;
        return false;
    }
    main_screen_ = lv_scr_act();
    visual_initialized_ = true;
    visual_mode_enabled_ = true;
    startEventLoop();
    std::cout << "=== Visual Test Mode Ready ===\n";
    return true;
}

bool VisualTestCoordinator::isVisualModeEnabled() const {
    return visual_mode_enabled_;
}

void VisualTestCoordinator::startEventLoop() {
    if (event_loop_running_.load()) return;
    should_stop_loop_.store(false);
    event_thread_ = std::make_unique<std::thread>(&VisualTestCoordinator::eventLoopFunction, this);
    event_loop_running_.store(true);
    std::cout << "Event loop thread started\n";
}

void VisualTestCoordinator::stopEventLoop() {
    if (!event_loop_running_.load()) return;
    std::cout << "Stopping event loop thread...\n";
    postTask([this] { should_stop_loop_.store(true); });
    if (event_thread_ && event_thread_->joinable()) {
        event_thread_->join();
    }
    event_loop_running_.store(false);
    event_thread_.reset();
    std::cout << "Event loop thread stopped\n";
}

void VisualTestCoordinator::postTask(std::function<void()> task) {
    if (!visual_mode_enabled_) return;
    {
        std::lock_guard<std::mutex> lock(task_queue_mutex_);
        task_queue_.emplace_back(std::move(task));
    }
    task_queue_cv_.notify_one();
}

void VisualTestCoordinator::postTaskSync(std::function<void()> task) {
    if (!visual_mode_enabled_) return;
    auto promise = std::make_shared<std::promise<void>>();
    auto future = promise->get_future();
    postTask([task = std::move(task), promise] {
        task();
        promise->set_value();
    });
    future.wait();
}

void VisualTestCoordinator::eventLoopFunction() {
    std::vector<std::function<void()>> local_task_queue;
    while (!should_stop_loop_.load()) {
        // Process LVGL timer handler (includes input events and drawing)
        bool completed = false;
        if (visual_mode_enabled_) {
            completed = lv_wayland_timer_handler();
        }

        // Process our tasks when LVGL is not rendering
        {
            std::unique_lock<std::mutex> lock(task_queue_mutex_);
            if (task_queue_cv_.wait_for(lock, std::chrono::milliseconds(1), 
                                       [this] { return !task_queue_.empty() || should_stop_loop_.load(); })) {
                local_task_queue = std::move(task_queue_);
            }
        }

        for (const auto& task : local_task_queue) {
            task();
        }
        local_task_queue.clear();

        // Use the same timing as main application
        if (completed) {
            // LV_DEF_REFR_PERIOD is typically 16ms (60 FPS)
            usleep(16000); // Wait for next refresh cycle when LVGL cycle completed
        } else {
            // Shorter wait when LVGL is still processing
            usleep(1000); // 1ms sleep to prevent busy-waiting
        }
    }
    std::cout << "Event loop thread exiting\n";
}

void VisualTestCoordinator::finalCleanup() {
    if (visual_initialized_) {
        std::cout << "\n=== Visual Test Mode Cleanup ===\n";
        stopEventLoop();
        visual_initialized_ = false;
        visual_mode_enabled_ = false;
    }
}

VisualTestCoordinator::~VisualTestCoordinator() {
    finalCleanup();
}

// VisualTestEnvironment implementation
void VisualTestEnvironment::SetUp() {
    // Initialize visual mode first
    VisualTestCoordinator::getInstance().initializeVisualMode();
    
    // Configure universal test settings
    if (debug_logging_enabled_) {
        spdlog::set_level(spdlog::level::debug);
        std::cout << "=== Universal Test Configuration ===\n";
        std::cout << "✓ Debug logging enabled\n";
    }
    
    // Display universal physics defaults
    std::cout << "✓ Default physics settings for ALL tests:\n";
    if (adhesion_disabled_by_default_) {
        std::cout << "  - Adhesion: DISABLED by default (tests must enable explicitly)\n";
    }
    if (cohesion_disabled_by_default_) {
        std::cout << "  - Cohesion: DISABLED by default (tests must enable explicitly)\n";
    }
    if (pressure_disabled_by_default_) {
        std::cout << "  - Pressure: DISABLED by default (tests must enable explicitly)\n";
    }
    if (ascii_logging_enabled_) {
        std::cout << "  - ASCII logging: ENABLED for world state visualization\n";
    }
    std::cout << "=====================================\n";
}

void VisualTestEnvironment::TearDown() {
    VisualTestCoordinator::getInstance().finalCleanup();
}

static ::testing::Environment* const visual_test_env =
    ::testing::AddGlobalTestEnvironment(new VisualTestEnvironment());

// VisualTestBase implementation
void VisualTestBase::SetUp() {
    auto& coordinator = VisualTestCoordinator::getInstance();
    visual_mode_ = coordinator.isVisualModeEnabled();
    const ::testing::TestInfo* test_info = ::testing::UnitTest::GetInstance()->current_test_info();
    current_test_name_ = std::string(test_info->test_case_name()) + "." + std::string(test_info->name());
    std::cout << "\n=== Starting Test: " << current_test_name_ << " ===\n";
    
    // Reset restart state for each test
    restart_enabled_ = false;
    restart_requested_ = false;

    if (visual_mode_) {
        // Store original cell size for restoration
        if (original_cell_size_ == -1) {
            original_cell_size_ = Cell::WIDTH;
        }
        
        coordinator.postTaskSync([this, &coordinator] {
            lv_obj_clean(lv_scr_act());
            ui_ = std::make_unique<TestUI>(lv_scr_act(), current_test_name_);
            ui_->initialize();
            // Ensure UI starts in non-restart mode
            ui_->setRestartMode(false);
        });
    }
}

void VisualTestBase::TearDown() {
    auto& coordinator = VisualTestCoordinator::getInstance();
    
    // Log final world state if ASCII logging enabled and we have a UI with a world
    if (VisualTestEnvironment::isAsciiLoggingEnabled() && ui_ && ui_->getWorld()) {
        if (ui_->getWorld()->getWorldType() == WorldType::RulesB) {
            logWorldStateAscii(static_cast<const WorldB*>(ui_->getWorld()), "Final world state");
        } else {
            logWorldStateAscii(static_cast<const World*>(ui_->getWorld()), "Final world state");
        }
    }
    
    if (visual_mode_ && ui_) {
        coordinator.postTaskSync([this] {
            ui_.reset();
        });
        
        // Restore original cell size after test
        if (auto_scaling_enabled_ && original_cell_size_ != -1) {
            restoreOriginalCellSize();
        }
    }
    std::cout << "=== Test " << current_test_name_ << " completed ===\n";
}

std::unique_ptr<World> VisualTestBase::createWorld(uint32_t width, uint32_t height) {
    // Apply auto-scaling before world creation
    if (visual_mode_ && auto_scaling_enabled_) {
        scaleDrawingAreaForWorld(width, height);
    }
    
    std::unique_ptr<World> world;
    if (visual_mode_) {
        auto& coordinator = VisualTestCoordinator::getInstance();
        coordinator.postTaskSync([&] {
            lv_obj_t* draw_area = ui_ ? ui_->getDrawArea() : nullptr;
            world = std::make_unique<World>(width, height, draw_area);
            if (ui_) {
                ui_->setWorld(world.get());
            }
        });
    } else {
        world = std::make_unique<World>(width, height, nullptr);
    }
    return world;
}

std::unique_ptr<WorldB> VisualTestBase::createWorldB(uint32_t width, uint32_t height) {
    // Apply auto-scaling before world creation
    if (visual_mode_ && auto_scaling_enabled_) {
        scaleDrawingAreaForWorld(width, height);
    }
    
    std::unique_ptr<WorldB> world;
    if (visual_mode_) {
        auto& coordinator = VisualTestCoordinator::getInstance();
        coordinator.postTaskSync([&] {
            lv_obj_t* draw_area = ui_ ? ui_->getDrawArea() : nullptr;
            world = std::make_unique<WorldB>(width, height, draw_area);
            if (ui_) {
                ui_->setWorld(world.get());
            }
        });
    } else {
        world = std::make_unique<WorldB>(width, height, nullptr);
    }
    
    // Apply universal physics defaults
    if (world) {
        applyUniversalPhysicsDefaults(world.get());
    }
    
    return world;
}

void VisualTestBase::runSimulation(World* world, int steps, const std::string& description) {
    if (visual_mode_) {
        std::cout << "  Running visual simulation: " << description << " (" << steps << " steps)\n";
        auto& coordinator = VisualTestCoordinator::getInstance();
        double deltaTime = 0.016;
        
        for (int i = 0; i < steps; ++i) {
            world->advanceTime(deltaTime);
            
            // Update visuals for every frame to ensure we see all physics behavior
            coordinator.postTaskSync([world, this, description, i, steps] {
                world->draw();
                // Update label with current progress
                std::string status = current_test_name_ + " - " + description +
                                     " [" + std::to_string(i + 1) + "/" + std::to_string(steps) + "]";
                if(ui_) ui_->updateTestLabel(status);
            });
            
            // Small delay to make the animation visible (approximately 60 FPS)
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
        }
    } else {
        double deltaTime = 0.016;
        for (int i = 0; i < steps; ++i) {
            world->advanceTime(deltaTime);
        }
    }
}

void VisualTestBase::waitForStart() {
    if (visual_mode_ && ui_) {
        auto& coordinator = VisualTestCoordinator::getInstance();
        // Reset button states and update UI on LVGL thread
        coordinator.postTaskSync([this] {
            ui_->start_pressed_.store(false);
            
            if (restart_enabled_) {
                ui_->setRestartMode(true);
                ui_->updateButtonStatus("Press Start to begin or restart test");
            } else {
                ui_->setRestartMode(false);
                ui_->updateButtonStatus("Press Start to begin test");
            }
        });
        
        // Wait on the test thread (not LVGL thread) to avoid blocking events
        while (!ui_->start_pressed_.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        
        spdlog::info("[TEST] Start button pressed!");
        
        // Check if restart was requested after button press
        if (restart_enabled_) {
            restart_requested_ = ui_->restart_requested_.load();
            spdlog::info("[TEST] Restart requested: {}", restart_requested_);
        }
        
        spdlog::info("[TEST] Exiting waitForStart() - restart_enabled_={}, restart_requested_={}", 
                     restart_enabled_, restart_requested_);
    } else {
        spdlog::info("[TEST] waitForStart() - non-visual mode, continuing immediately");
    }
}

VisualTestBase::TestAction VisualTestBase::waitForStartOrStep() {
    if (visual_mode_ && ui_) {
        auto& coordinator = VisualTestCoordinator::getInstance();
        // Reset button states and update UI on LVGL thread
        coordinator.postTaskSync([this] {
            ui_->start_pressed_.store(false);
            ui_->step_pressed_.store(false);
            ui_->enableStepButton();  // Enable Step button alongside Start
            ui_->updateButtonStatus("Press Start to run or Step to advance manually");
        });
        
        // Wait on the test thread for either button to be pressed
        while (!ui_->start_pressed_.load() && !ui_->step_pressed_.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        
        if (ui_->start_pressed_.load()) {
            spdlog::info("[TEST] Start button pressed - running continuously");
            
            // Disable Step button when Start is pressed
            coordinator.postTaskSync([this] {
                ui_->disableStepButton();
                ui_->updateButtonStatus("Running test continuously...");
            });
            
            return TestAction::START;
        } else {
            spdlog::info("[TEST] Step button pressed - entering step mode");
            
            // Keep Start button enabled for switching to continuous mode
            // Just enable step mode without disabling Start
            coordinator.postTaskSync([this] {
                ui_->setStepMode(true);
                ui_->updateButtonStatus("Step mode active - press Step to advance, Start to run continuously");
            });
            
            return TestAction::STEP;
        }
    } else {
        spdlog::info("[TEST] waitForStartOrStep() - non-visual mode, defaulting to START");
        return TestAction::START;
    }
}

void VisualTestBase::waitForNext() {
    if (visual_mode_ && ui_) {
        auto& coordinator = VisualTestCoordinator::getInstance();
        // Reset button state and enable Next button on LVGL thread
        coordinator.postTaskSync([this] {
            ui_->next_pressed_.store(false);
            ui_->enableNextButton();
            ui_->updateButtonStatus("Press Next to continue");
        });
        
        // Wait on the test thread (not LVGL thread) to avoid blocking events
        while (!ui_->next_pressed_.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        
        spdlog::info("[TEST] Next button pressed");
        
        // Disable Next button after press
        coordinator.postTaskSync([this] {
            ui_->disableNextButton();
        });
    }
}

void VisualTestBase::pauseIfVisual(int milliseconds) {
    if (visual_mode_) {
        std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
    }
}

void VisualTestBase::stepSimulation(World* world, int steps) {
    if (!world) return;
    
    if (visual_mode_) {
        auto& coordinator = VisualTestCoordinator::getInstance();
        
        for (int i = 0; i < steps; ++i) {
            world->advanceTime(0.016); // ~60 FPS timestep
            
            // Update visual display for each step
            coordinator.postTaskSync([world] {
                world->draw();
            });
            
            // Small delay to make step visible
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        
        // Update UI with step information
        if (ui_) {
            std::string status = current_test_name_ + " - Advanced " + std::to_string(steps) + " step(s)";
            coordinator.postTaskSync([this, status] {
                ui_->updateTestLabel(status);
            });
        }
    } else {
        // Non-visual mode: just advance time
        double deltaTime = 0.016;
        for (int i = 0; i < steps; ++i) {
            world->advanceTime(deltaTime);
        }
    }
}

VisualTestBase::TestAction VisualTestBase::waitForStep() {
    if (visual_mode_ && ui_) {
        spdlog::info("[TEST] Waiting for Step, Start (continue), or Next button press");
        
        auto& coordinator = VisualTestCoordinator::getInstance();
        // Reset button flags and ensure buttons are enabled
        coordinator.postTaskSync([this] {
            ui_->step_pressed_.store(false);
            ui_->start_pressed_.store(false);
            ui_->next_pressed_.store(false);
            ui_->enableStepButton();
            ui_->enableStartButton();  // Keep Start enabled to allow running to completion
            ui_->enableNextButton();   // Keep Next enabled to allow skipping
            ui_->updateButtonStatus("Press Step to advance, Start to run continuously, or Next to skip");
        });
        
        // Wait on the test thread for any button to be pressed
        while (!ui_->step_pressed_.load() && 
               !ui_->start_pressed_.load() && 
               !ui_->next_pressed_.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        
        if (ui_->step_pressed_.load()) {
            spdlog::info("[TEST] Step button pressed - continuing step mode");
            return TestAction::STEP;
        } else if (ui_->start_pressed_.load()) {
            spdlog::info("[TEST] Start button pressed - switching to continuous mode");
            
            // Update UI to reflect continuous mode
            coordinator.postTaskSync([this] {
                ui_->setStepMode(false);  // Exit step mode
                ui_->disableStepButton();
                ui_->updateButtonStatus("Running continuously...");
            });
            
            return TestAction::START;
        } else {
            spdlog::info("[TEST] Next button pressed - skipping test");
            
            // Disable buttons when skipping
            coordinator.postTaskSync([this] {
                ui_->disableStepButton();
                ui_->disableNextButton();
                ui_->updateButtonStatus("Skipping to next test...");
            });
            
            return TestAction::NEXT;
        }
    } else {
        spdlog::info("[TEST] waitForStep() - non-visual mode, defaulting to STEP");
        return TestAction::STEP;
    }
}

void VisualTestBase::scaleDrawingAreaForWorld(uint32_t world_width, uint32_t world_height) {
    if (!visual_mode_ || !auto_scaling_enabled_) return;
    
    // Get available drawing area size from TestUI
    const int draw_area_size = TestUI::DRAW_AREA_SIZE;  // 400 pixels
    
    // Calculate optimal cell size to fit the world in the drawing area
    // Leave some margin by using 90% of available space
    const int usable_area = static_cast<int>(draw_area_size * 0.9);
    
    int optimal_cell_size_x = usable_area / world_width;
    int optimal_cell_size_y = usable_area / world_height;
    int optimal_cell_size = std::min(optimal_cell_size_x, optimal_cell_size_y);
    
    // Ensure minimum readable cell size (at least 20 pixels)
    optimal_cell_size = std::max(optimal_cell_size, 20);
    
    // Ensure maximum cell size doesn't exceed reasonable bounds (200 pixels)
    optimal_cell_size = std::min(optimal_cell_size, 200);
    
    std::cout << "Auto-scaling: World " << world_width << "x" << world_height 
              << " → Cell size " << optimal_cell_size << " pixels\n";
    
    auto& coordinator = VisualTestCoordinator::getInstance();
    coordinator.postTaskSync([optimal_cell_size] {
        Cell::setSize(optimal_cell_size);
    });
}

void VisualTestBase::restoreOriginalCellSize() {
    if (original_cell_size_ != -1) {
        auto& coordinator = VisualTestCoordinator::getInstance();
        coordinator.postTaskSync([this] {
            Cell::setSize(original_cell_size_);
        });
    }
}

void VisualTestBase::applyUniversalPhysicsDefaults(WorldB* world) {
    if (!world) return;
    
    spdlog::debug("[TEST] Applying universal physics defaults to WorldB");
    
    // Apply adhesion defaults
    if (VisualTestEnvironment::isAdhesionDisabledByDefault()) {
        world->setAdhesionEnabled(false);
        world->setAdhesionStrength(0.0);
        spdlog::debug("[TEST] - Adhesion disabled by default");
    }
    
    // Apply cohesion defaults
    if (VisualTestEnvironment::isCohesionDisabledByDefault()) {
        world->setCohesionBindForceEnabled(false);
        world->setCohesionComForceEnabled(false);
        world->setCohesionComForceStrength(0.0);
        world->setCohesionBindForceStrength(0.0);
        spdlog::debug("[TEST] - All cohesion systems disabled by default");
    }
    
    // Apply pressure defaults
    if (VisualTestEnvironment::isPressureDisabledByDefault()) {
        world->setHydrostaticPressureEnabled(false);
        world->setDynamicPressureEnabled(false);
        world->setPressureScale(0.0);
        spdlog::debug("[TEST] - All pressure systems disabled by default");
    }
}

void VisualTestBase::logWorldStateAscii(const WorldB* world, const std::string& description) {
    if (!world || !VisualTestEnvironment::isAsciiLoggingEnabled()) return;
    
    std::string ascii = world->toAsciiDiagram();
    spdlog::debug("[TEST ASCII] {}\n{}", description, ascii);
}

void VisualTestBase::logWorldStateAscii(const World* world, const std::string& description) {
    if (!world || !VisualTestEnvironment::isAsciiLoggingEnabled()) return;
    
    std::string ascii = world->toAsciiDiagram();
    spdlog::debug("[TEST ASCII] {}\n{}", description, ascii);
}

void VisualTestBase::logInitialTestState(const WorldB* world, const std::string& test_description) {
    if (!world || !VisualTestEnvironment::isAsciiLoggingEnabled()) return;
    
    std::string description = test_description.empty() ? "Initial test state" : test_description;
    std::string ascii = world->toAsciiDiagram();
    spdlog::info("[TEST SETUP] {}\n{}", description, ascii);
}

void VisualTestBase::logInitialTestState(const World* world, const std::string& test_description) {
    if (!world || !VisualTestEnvironment::isAsciiLoggingEnabled()) return;
    
    std::string description = test_description.empty() ? "Initial test state" : test_description;
    std::string ascii = world->toAsciiDiagram();
    spdlog::info("[TEST SETUP] {}\n{}", description, ascii);
}

void VisualTestBase::logInitialTestState(const WorldInterface* world, const std::string& test_description) {
    if (!world || !VisualTestEnvironment::isAsciiLoggingEnabled()) return;
    
    // Try to cast to specific implementations to get ASCII diagram
    if (auto* worldB = dynamic_cast<const WorldB*>(world)) {
        logInitialTestState(worldB, test_description);
    } else if (auto* worldA = dynamic_cast<const World*>(world)) {
        logInitialTestState(worldA, test_description);
    }
}

void VisualTestBase::logWorldState(const WorldB* world, const std::string& context) {
    if (!world) return;
    
    spdlog::debug("=== World State: {} ===", context);
    double totalMass = 0.0;
    const double PRESSURE_LOG_THRESHOLD = 0.0001;  // Very small threshold for pressure logging
    
    for (uint32_t y = 0; y < world->getHeight(); y++) {
        for (uint32_t x = 0; x < world->getWidth(); x++) {
            const CellB& cell = world->at(x, y);
            if (cell.getFillRatio() > 0.001) {  // Only log cells with meaningful mass
                // Check if cell has any significant pressure
                double dynamicPressure = cell.getDynamicPressure();
                double hydrostaticPressure = cell.getHydrostaticPressure();
                double debugPressure = cell.getDebugPressureMagnitude();  // Pressure before it was consumed
                
                if (dynamicPressure > PRESSURE_LOG_THRESHOLD || hydrostaticPressure > PRESSURE_LOG_THRESHOLD || debugPressure > PRESSURE_LOG_THRESHOLD) {
                    // Log with pressure information
                    spdlog::debug("  Cell({},{}) - Material: {}, Fill: {:.6f}, Velocity: ({:.3f},{:.3f}), COM: ({:.3f},{:.3f}), DynP: {:.6f}, HydroP: {:.6f}, DebugP: {:.6f}",
                                 x, y, 
                                 getMaterialName(cell.getMaterialType()),
                                 cell.getFillRatio(),
                                 cell.getVelocity().x, cell.getVelocity().y,
                                 cell.getCOM().x, cell.getCOM().y,
                                 dynamicPressure, hydrostaticPressure, debugPressure);
                } else {
                    // Log without pressure (original format)
                    spdlog::debug("  Cell({},{}) - Material: {}, Fill: {:.6f}, Velocity: ({:.3f},{:.3f}), COM: ({:.3f},{:.3f})",
                                 x, y, 
                                 getMaterialName(cell.getMaterialType()),
                                 cell.getFillRatio(),
                                 cell.getVelocity().x, cell.getVelocity().y,
                                 cell.getCOM().x, cell.getCOM().y);
                }
                totalMass += cell.getFillRatio();
            }
        }
    }
    spdlog::debug("  Total mass in world: {:.6f}", totalMass);
}

// Enhanced visual test helpers implementation
void VisualTestBase::updateDisplay(WorldInterface* world, const std::string& status) {
    if (!visual_mode_ || !world) return;
    
    auto& coordinator = VisualTestCoordinator::getInstance();
    coordinator.postTaskSync([this, world, status] {
        world->draw();
        if (ui_ && !status.empty()) {
            ui_->updateButtonStatus(status);
        }
    });
    
    // Small delay to ensure visual update is visible
    pauseIfVisual(50);
}

void VisualTestBase::updateDisplayNoDelay(WorldInterface* world, const std::string& status) {
    if (!visual_mode_ || !world) return;
    
    auto& coordinator = VisualTestCoordinator::getInstance();
    coordinator.postTaskSync([this, world, status] {
        world->draw();
        if (ui_ && !status.empty()) {
            ui_->updateButtonStatus(status);
        }
    });
}

void VisualTestBase::showInitialState(WorldInterface* world, const std::string& description) {
    if (!world) return;
    
    // Log initial state with ASCII diagram
    logInitialTestState(world, description);
    
    if (visual_mode_) {
        // Disable restart for simple test flow (MUST happen before waitForStart)
        disableTestRestart();
        
        // Update visual display
        updateDisplay(world, "Initial state: " + description);
        
        // Wait for user to start
        waitForStart();
    }
}

void VisualTestBase::showInitialStateWithStep(WorldInterface* world, const std::string& description) {
    if (!world) return;
    
    // Log initial state with ASCII diagram
    logInitialTestState(world, description);
    
    if (visual_mode_) {
        // Disable restart for simple test flow
        disableTestRestart();
        
        // Update visual display
        updateDisplay(world, "Initial state: " + description);
        
        // Wait for user to choose Start or Step
        TestAction action = waitForStartOrStep();
        
        // Store the chosen action for later use in stepSimulation
        if (action == TestAction::STEP) {
            // Enable step mode in UI
            auto& coordinator = VisualTestCoordinator::getInstance();
            coordinator.postTaskSync([this] {
                ui_->setStepMode(true);
            });
        }
    }
}

void VisualTestBase::stepSimulation(WorldInterface* world, int steps, const std::string& stepDescription) {
    if (!world) return;
    
    if (visual_mode_) {
        auto& coordinator = VisualTestCoordinator::getInstance();
        
        for (int i = 0; i < steps; ++i) {
            // In step mode, wait for user input BEFORE advancing
            if (ui_ && ui_->isStepModeEnabled()) {
                // Update status to show what's about to happen
                std::string preStepStatus = stepDescription.empty() ? 
                    "Ready for step " + std::to_string(i + 1) + "/" + std::to_string(steps) + " - press Step" :
                    stepDescription + " [" + std::to_string(i + 1) + "/" + std::to_string(steps) + "] - press Step";
                
                coordinator.postTaskSync([this, preStepStatus] {
                    if (ui_) {
                        ui_->updateButtonStatus(preStepStatus);
                    }
                });
                
                TestAction action = waitForStep();
                
                // Handle the action
                if (action == TestAction::START) {
                    // User wants to run continuously - exit step mode and run remaining steps
                    spdlog::info("[TEST] Switching from step mode to continuous mode");
                    
                    // Run remaining steps continuously
                    for (int j = i; j < steps; ++j) {
                        world->advanceTime(0.016);
                        
                        std::string status = stepDescription.empty() ? 
                            "Step " + std::to_string(j + 1) + "/" + std::to_string(steps) :
                            stepDescription + " [" + std::to_string(j + 1) + "/" + std::to_string(steps) + "]";
                        
                        coordinator.postTaskSync([this, world, status] {
                            world->draw();
                            if (ui_) {
                                ui_->updateButtonStatus(status);
                            }
                        });
                        
                        pauseIfVisual(100);
                    }
                    
                    // Exit the outer loop since we've completed all steps
                    break;
                } else if (action == TestAction::NEXT) {
                    // User wants to skip - exit immediately
                    spdlog::info("[TEST] Skipping remaining steps");
                    return;
                }
                // Otherwise action == TestAction::STEP, continue stepping
            }
            
            // Advance physics
            world->advanceTime(0.016); // ~60 FPS timestep
            
            // Update display with progress
            std::string status = stepDescription.empty() ? 
                "Step " + std::to_string(i + 1) + "/" + std::to_string(steps) + " completed" :
                stepDescription + " [" + std::to_string(i + 1) + "/" + std::to_string(steps) + "]";
            
            coordinator.postTaskSync([this, world, status] {
                world->draw();
                if (ui_) {
                    ui_->updateButtonStatus(status);
                }
            });
            
            // In continuous mode, add small pause to make steps visible
            if (!ui_ || !ui_->isStepModeEnabled()) {
                pauseIfVisual(100);
            }
        }
        
        // Final status update
        if (ui_) {
            std::string finalStatus = stepDescription.empty() ?
                "Completed " + std::to_string(steps) + " steps" :
                stepDescription + " - Complete";
            
            coordinator.postTaskSync([this, finalStatus] {
                ui_->updateButtonStatus(finalStatus);
            });
        }
    } else {
        // Non-visual mode: just advance time
        double deltaTime = 0.016;
        for (int i = 0; i < steps; ++i) {
            world->advanceTime(deltaTime);
        }
    }
}

void VisualTestBase::runContinuousSimulation(WorldInterface* world, int steps, const std::string& description) {
    if (!world) return;
    
    if (visual_mode_) {
        // If in step mode, use stepSimulation behavior instead
        if (ui_ && ui_->isStepModeEnabled()) {
            stepSimulation(world, steps, description);
            return;
        }
        
        auto& coordinator = VisualTestCoordinator::getInstance();
        double deltaTime = 0.016; // ~60 FPS
        
        for (int i = 0; i < steps; ++i) {
            // Advance physics
            world->advanceTime(deltaTime);
            
            // Update display immediately without delay
            std::string status = description.empty() ? 
                "Step " + std::to_string(i + 1) + "/" + std::to_string(steps) :
                description + " [" + std::to_string(i + 1) + "/" + std::to_string(steps) + "]";
            
            coordinator.postTaskSync([this, world, status] {
                world->draw();
                if (ui_) {
                    ui_->updateButtonStatus(status);
                }
            });
            
            // Sleep for consistent frame rate (16ms = ~60 FPS)
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
        }
        
        // Final status update
        if (ui_) {
            std::string finalStatus = description.empty() ?
                "Completed " + std::to_string(steps) + " steps" :
                description + " - Complete";
            
            coordinator.postTaskSync([this, finalStatus] {
                ui_->updateButtonStatus(finalStatus);
            });
        }
    } else {
        // Non-visual mode: just advance time
        double deltaTime = 0.016;
        for (int i = 0; i < steps; ++i) {
            world->advanceTime(deltaTime);
        }
    }
}

// Utility function for backward compatibility
bool isVisualModeEnabled() {
    return VisualTestCoordinator::getInstance().isVisualModeEnabled();
} 
