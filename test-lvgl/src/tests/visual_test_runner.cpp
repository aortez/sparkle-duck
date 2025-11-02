#include "visual_test_runner.h"
#include <future>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <sstream>
#include <iomanip>
#include "../World.h"

// External global settings used by the backend system.
extern simulator_settings_t settings;

// VisualTestEnvironment static member definitions.
bool VisualTestEnvironment::debug_logging_enabled_ = true;
bool VisualTestEnvironment::adhesion_disabled_by_default_ = true;
bool VisualTestEnvironment::cohesion_disabled_by_default_ = true;
bool VisualTestEnvironment::pressure_disabled_by_default_ = true;
bool VisualTestEnvironment::ascii_logging_enabled_ = true;

// VisualTestCoordinator implementation.
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
        // Process LVGL timer handler (includes input events and drawing).
        bool completed = false;
        if (visual_mode_enabled_) {
            completed = lv_wayland_timer_handler();
        }

        // Process our tasks when LVGL is not rendering.
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

        if (completed) {
        	// Don't wait.
        } else {
            // Shorter wait when LVGL is still processing.
            usleep(1000); // 1ms sleep to prevent busy-waiting.
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

// VisualTestEnvironment implementation.
void VisualTestEnvironment::SetUp() {
    // Initialize visual mode first.
    VisualTestCoordinator::getInstance().initializeVisualMode();
    
    // Configure universal test settings.
    if (debug_logging_enabled_) {
        // Set up file and console logging similar to main application.
        try {
            // Create console sink with colors.
            auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
            console_sink->set_level(spdlog::level::info); // Console at debug level.

            // Create basic file sink for tests (overwrites each run).
            auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(
                "test.log", true);                    // true = truncate (overwrite).
            file_sink->set_level(spdlog::level::trace); // Everything to file.

            // Create logger with both sinks.
            std::vector<spdlog::sink_ptr> sinks{ console_sink, file_sink };
            auto logger = std::make_shared<spdlog::logger>("test-runner", sinks.begin(), sinks.end());

            // Set as default logger.
            spdlog::set_default_logger(logger);
            spdlog::set_level(spdlog::level::trace); // Global level must be at least as low as any sink.
            spdlog::flush_every(std::chrono::seconds(1)); // Flush every second.
            
            std::cout << "=== Universal Test Configuration ===\n";
            std::cout << "✓ Debug logging enabled (console: debug, file: trace)\n";
            std::cout << "✓ Test logs written to: test.log\n";
        }
        catch (const spdlog::spdlog_ex& ex) {
            std::cerr << "Log initialization failed: " << ex.what() << "\n";
            // Fall back to simple console logging.
            spdlog::set_level(spdlog::level::debug);
            std::cout << "=== Universal Test Configuration ===\n";
            std::cout << "✓ Debug logging enabled (console only)\n";
        }
    }
    
    // Display universal physics defaults.
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
    // Ensure all logs are flushed to file before test exit.
    spdlog::shutdown();
}

static ::testing::Environment* const visual_test_env =
    ::testing::AddGlobalTestEnvironment(new VisualTestEnvironment());

// VisualTestBase implementation.
void VisualTestBase::SetUp() {
    auto& coordinator = VisualTestCoordinator::getInstance();
    visual_mode_ = coordinator.isVisualModeEnabled();
    const ::testing::TestInfo* test_info = ::testing::UnitTest::GetInstance()->current_test_info();
    current_test_name_ = std::string(test_info->test_case_name()) + "." + std::string(test_info->name());
    std::cout << "\n=== Starting Test: " << current_test_name_ << " ===\n";
    
    // Reset restart state for each test.
    restart_enabled_ = false;
    restart_requested_ = false;

    if (visual_mode_) {
        // Store original cell size for restoration.
        if (original_cell_size_ == -1) {
            original_cell_size_ = Cell::WIDTH;
        }
        
        // Clear test skip flag for new test.
        test_skipped_ = false;
        
        coordinator.postTaskSync([this, &coordinator] {
            lv_obj_clean(lv_scr_act());
            ui_ = std::make_unique<TestUI>(lv_scr_act(), current_test_name_);
            ui_->initialize();
            // Ensure UI starts in non-restart mode.
            ui_->setRestartMode(false);
        });
    }
}

void VisualTestBase::TearDown() {
    auto& coordinator = VisualTestCoordinator::getInstance();
    
    // Log final world state if ASCII logging enabled and we have a UI with a world.
    if (VisualTestEnvironment::isAsciiLoggingEnabled() && ui_ && ui_->getWorld()) {
        logWorldStateAscii(static_cast<const World*>(ui_->getWorld()), "Final world state");
    }
    
    if (visual_mode_ && ui_) {
        coordinator.postTaskSync([this] {
            ui_.reset();
        });
        
        // Restore original cell size after test.
        if (auto_scaling_enabled_ && original_cell_size_ != -1) {
            restoreOriginalCellSize();
        }
    }
    std::cout << "=== Test " << current_test_name_ << " completed ===\n";
}

std::unique_ptr<World> VisualTestBase::createWorld(uint32_t width, uint32_t height) {
    // Apply auto-scaling before world creation.
    if (visual_mode_ && auto_scaling_enabled_) {
        scaleDrawingAreaForWorld(width, height);
    }
    
    std::unique_ptr<World> world;
    if (visual_mode_) {
        auto& coordinator = VisualTestCoordinator::getInstance();
        coordinator.postTaskSync([&] {
            world = std::make_unique<World>(width, height);
            if (ui_) {
                ui_->setWorld(world.get());
            }
        });
    } else {
        world = std::make_unique<World>(width, height);
    }
    return world;
}

std::unique_ptr<World> VisualTestBase::createWorldB(uint32_t width, uint32_t height) {
    // Apply auto-scaling before world creation.
    if (visual_mode_ && auto_scaling_enabled_) {
        scaleDrawingAreaForWorld(width, height);
    }
    
    std::unique_ptr<World> world;
    if (visual_mode_) {
        auto& coordinator = VisualTestCoordinator::getInstance();
        coordinator.postTaskSync([&] {
            world = std::make_unique<World>(width, height);
            if (ui_) {
                ui_->setWorld(world.get());
            }
        });
    } else {
        world = std::make_unique<World>(width, height);
    }
    
    // Apply universal physics defaults.
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
            // Check if Next button was pressed during simulation.
            if (ui_ && ui_->next_pressed_.load()) {
                spdlog::info("[TEST] Next button pressed during simulation - skipping");
                test_skipped_ = true;
                coordinator.postTaskSync([this] {
                    ui_->disableNextButton();
                    ui_->updateButtonStatus("Test skipped");
                });
                return;
            }
            
            world->advanceTime(deltaTime);
            
            // Update visuals for every frame to ensure we see all physics behavior.
            coordinator.postTaskSync([world, this, description, i, steps] {
                if (ui_) {
                    world->draw(*ui_->getDrawArea());
                }
                // Update label with current progress.
                std::string status = current_test_name_ + " - " + description +
                                     " [" + std::to_string(i + 1) + "/" + std::to_string(steps) + "]";
                if(ui_) ui_->updateTestLabel(status);
            });
            
            // Small delay to make the animation visible (approximately 60 FPS).
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
        }
        
        // Enable restart after successful completion.
        if (ui_ && !test_skipped_) {
            enableRestartAfterCompletion();
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
        // Reset button states and update UI on LVGL thread.
        coordinator.postTaskSync([this] {
            ui_->start_pressed_.store(false);
            ui_->next_pressed_.store(false);
            ui_->enableNextButton();  // Enable Next button to allow skipping.
            
            if (restart_enabled_) {
                ui_->setRestartMode(true);
                ui_->updateButtonStatus("Press Start to begin/restart test or Next to skip to next test");
            } else {
                ui_->setRestartMode(false);
                ui_->updateButtonStatus("Press Start to begin test or Next to skip to next test");
            }
        });
        
        // Wait on the test thread (not LVGL thread) to avoid blocking events.
        while (!ui_->start_pressed_.load() && !ui_->next_pressed_.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        
        if (ui_->next_pressed_.load()) {
            spdlog::info("[TEST] Next button pressed - skipping to next test");
            test_skipped_ = true;
            
            // Disable Next button after skip.
            coordinator.postTaskSync([this] {
                ui_->disableNextButton();
                ui_->updateButtonStatus("Test skipped");
            });
            return;
        }
        
        spdlog::info("[TEST] Start button pressed!");
        test_skipped_ = false;
        
        // Check if restart was requested after button press.
        if (restart_enabled_) {
            restart_requested_ = ui_->restart_requested_.load();
            spdlog::info("[TEST] Restart requested: {}", restart_requested_);
        }
        
        spdlog::info("[TEST] Exiting waitForStart() - restart_enabled_={}, restart_requested_={}", 
                     restart_enabled_, restart_requested_);
    } else {
        spdlog::info("[TEST] waitForStart() - non-visual mode, continuing immediately");
        test_skipped_ = false;
    }
}

VisualTestBase::TestAction VisualTestBase::waitForStartOrStep() {
    if (visual_mode_ && ui_) {
        auto& coordinator = VisualTestCoordinator::getInstance();
        // Reset button states and update UI on LVGL thread.
        coordinator.postTaskSync([this] {
            ui_->start_pressed_.store(false);
            ui_->step_pressed_.store(false);
            ui_->next_pressed_.store(false);
            ui_->enableStepButton();  // Enable Step button alongside Start.
            ui_->enableNextButton();   // Enable Next button to allow skipping.
            ui_->updateButtonStatus("Press Start to run, Step to advance manually, or Next to skip test");
        });
        
        // Wait on the test thread for any button to be pressed.
        while (!ui_->start_pressed_.load() && !ui_->step_pressed_.load() && !ui_->next_pressed_.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        
        if (ui_->next_pressed_.load()) {
            spdlog::info("[TEST] Next button pressed - skipping to next test");
            test_skipped_ = true;
            
            // Disable buttons after skip.
            coordinator.postTaskSync([this] {
                ui_->disableNextButton();
                ui_->disableStepButton();
                ui_->updateButtonStatus("Test skipped");
            });
            return TestAction::NEXT;
        } else if (ui_->start_pressed_.load()) {
            spdlog::info("[TEST] Start button pressed - running continuously");
            test_skipped_ = false;
            
            // Disable Step button when Start is pressed.
            coordinator.postTaskSync([this] {
                ui_->disableStepButton();
                ui_->updateButtonStatus("Running test continuously...");
            });
            
            return TestAction::START;
        } else {
            spdlog::info("[TEST] Step button pressed - entering step mode");
            test_skipped_ = false;
            
            // Keep Start button enabled for switching to continuous mode.
            // Just enable step mode without disabling Start.
            coordinator.postTaskSync([this] {
                ui_->setStepMode(true);
                ui_->updateButtonStatus("Step mode active - press Step to advance, Start to run continuously");
            });
            
            return TestAction::STEP;
        }
    } else {
        spdlog::info("[TEST] waitForStartOrStep() - non-visual mode, defaulting to START");
        test_skipped_ = false;
        return TestAction::START;
    }
}

void VisualTestBase::waitForNext() {
    if (visual_mode_ && ui_) {
        auto& coordinator = VisualTestCoordinator::getInstance();
        // Reset button state and enable Next button on LVGL thread.
        coordinator.postTaskSync([this] {
            ui_->next_pressed_.store(false);
            ui_->enableNextButton();
            ui_->updateButtonStatus("Press Next to continue");
        });
        
        // Wait on the test thread (not LVGL thread) to avoid blocking events.
        while (!ui_->next_pressed_.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        
        spdlog::info("[TEST] Next button pressed");
        
        // Disable Next button after press.
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

void VisualTestBase::enableRestartAfterCompletion() {
    if (visual_mode_ && ui_ && !test_skipped_) {
        auto& coordinator = VisualTestCoordinator::getInstance();
        coordinator.postTaskSync([this] {
            // Enable restart functionality.
            ui_->setRestartMode(true);
            ui_->enableStartButton();
            ui_->start_pressed_.store(false);
            ui_->updateButtonStatus("Test complete - Press Start to restart or Next to continue");
        });
        
        // Set restart enabled flag so the test can be rerun.
        restart_enabled_ = true;
    }
}

bool VisualTestBase::waitForRestartOrNext() {
    if (!visual_mode_ || !ui_) {
        return false;  // No restart in non-visual mode.
    }
    
    // First enable restart functionality.
    enableRestartAfterCompletion();
    
    auto& coordinator = VisualTestCoordinator::getInstance();
    
    // Reset button states.
    coordinator.postTaskSync([this] {
        ui_->start_pressed_.store(false);
        ui_->next_pressed_.store(false);
        ui_->restart_requested_.store(false);
    });
    
    // Wait for either Start (restart) or Next button.
    while (!ui_->start_pressed_.load() && !ui_->next_pressed_.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    
    if (ui_->next_pressed_.load()) {
        spdlog::info("[TEST] Next button pressed - moving to next test");
        coordinator.postTaskSync([this] {
            ui_->disableNextButton();
            ui_->updateButtonStatus("Moving to next test...");
        });
        return false;  // Don't restart.
    } else if (ui_->start_pressed_.load()) {
        spdlog::info("[TEST] Start button pressed - restarting test");
        restart_requested_ = true;
        coordinator.postTaskSync([this] {
            ui_->updateButtonStatus("Restarting test...");
            // Keep Start button enabled for next restart.
        });
        return true;  // Restart the test.
    }
    
    return false;
}

void VisualTestBase::stepSimulation(World* world, int steps) {
    if (!world) return;
    
    if (visual_mode_) {
        auto& coordinator = VisualTestCoordinator::getInstance();
        
        for (int i = 0; i < steps; ++i) {
            world->advanceTime(0.016); // ~60 FPS timestep.
            
            // Update visual display for each step.
            coordinator.postTaskSync([world, this] {
                if (ui_) {
                    world->draw(*ui_->getDrawArea());
                }
            });
            
            // Small delay to make step visible.
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        
        // Update UI with step information.
        if (ui_) {
            std::string status = current_test_name_ + " - Advanced " + std::to_string(steps) + " step(s)";
            coordinator.postTaskSync([this, status] {
                ui_->updateTestLabel(status);
            });
        }
    } else {
        // Non-visual mode: just advance time.
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
        // Reset button flags and ensure buttons are enabled.
        coordinator.postTaskSync([this] {
            ui_->step_pressed_.store(false);
            ui_->start_pressed_.store(false);
            ui_->next_pressed_.store(false);
            ui_->enableStepButton();
            ui_->enableStartButton();  // Keep Start enabled to allow running to completion.
            ui_->enableNextButton();   // Keep Next enabled to allow skipping.
            ui_->updateButtonStatus("Press Step to advance, Start to run continuously, or Next to skip");
        });
        
        // Wait on the test thread for any button to be pressed.
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
            
            // Update UI to reflect continuous mode.
            coordinator.postTaskSync([this] {
                ui_->setStepMode(false);  // Exit step mode.
                ui_->disableStepButton();
                ui_->updateButtonStatus("Running continuously...");
            });
            
            return TestAction::START;
        } else {
            spdlog::info("[TEST] Next button pressed - skipping test");
            test_skipped_ = true;
            
            // Disable buttons when skipping.
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
    
    // Get available drawing area size from TestUI.
    const int draw_area_size = TestUI::DRAW_AREA_SIZE;  // 400 pixels.
    
    // Calculate optimal cell size to fit the world in the drawing area.
    // Leave some margin by using 90% of available space.
    const int usable_area = static_cast<int>(draw_area_size * 0.9);
    
    int optimal_cell_size_x = usable_area / world_width;
    int optimal_cell_size_y = usable_area / world_height;
    int optimal_cell_size = std::min(optimal_cell_size_x, optimal_cell_size_y);
    
    // Ensure minimum readable cell size (at least 20 pixels).
    optimal_cell_size = std::max(optimal_cell_size, 20);
    
    // Ensure maximum cell size doesn't exceed reasonable bounds (200 pixels).
    optimal_cell_size = std::min(optimal_cell_size, 200);
    
    std::cout << "Auto-scaling: World " << world_width << "x" << world_height 
              << " → Cell size " << optimal_cell_size << " pixels\n";
    
    // TODO: Cell size is now constant (Cell::WIDTH/HEIGHT).
    // Scaling functionality needs to be reimplemented differently.
    spdlog::debug("scaleDrawingAreaForWorld: cell size is now constant at {}px", Cell::WIDTH);
}

void VisualTestBase::restoreOriginalCellSize() {
    // TODO: Cell size is now constant - no restoration needed.
    spdlog::debug("restoreOriginalCellSize: no-op (cell size is constant)");
}

void VisualTestBase::applyUniversalPhysicsDefaults(World* world) {
    if (!world) return;
    
    spdlog::debug("[TEST] Applying universal physics defaults to World");
    
    // Apply adhesion defaults.
    if (VisualTestEnvironment::isAdhesionDisabledByDefault()) {
        world->setAdhesionEnabled(false);
        world->setAdhesionStrength(0.0);
        spdlog::debug("[TEST] - Adhesion disabled by default");
    }
    
    // Apply cohesion defaults.
    if (VisualTestEnvironment::isCohesionDisabledByDefault()) {
        world->setCohesionBindForceEnabled(false);
        world->setCohesionComForceEnabled(false);
        world->setCohesionComForceStrength(0.0);
        world->setCohesionBindForceStrength(0.0);
        spdlog::debug("[TEST] - All cohesion systems disabled by default");
    }
    
    // Apply pressure defaults.
    if (VisualTestEnvironment::isPressureDisabledByDefault()) {
        world->setHydrostaticPressureEnabled(false);
        world->setDynamicPressureEnabled(false);
        world->setPressureScale(0.0);
        spdlog::debug("[TEST] - All pressure systems disabled by default");
    }
}

void VisualTestBase::logWorldStateAscii(const World* world, const std::string& description) {
    if (!world || !VisualTestEnvironment::isAsciiLoggingEnabled()) return;

    std::string ascii = world->toAsciiDiagram();
    spdlog::debug("[TEST ASCII] {}\n{}", description, ascii);
}

void VisualTestBase::logInitialTestState(const World* world, const std::string& test_description) {
    if (!world || !VisualTestEnvironment::isAsciiLoggingEnabled()) return;
    
    std::string description = test_description.empty() ? "Initial test state" : test_description;
    std::string ascii = world->toAsciiDiagram();
    spdlog::info("[TEST SETUP] {}\n{}", description, ascii);
}

void VisualTestBase::logInitialTestState(const World* world, const std::string& test_description) {
    if (!world || !VisualTestEnvironment::isAsciiLoggingEnabled()) return;

    // Cast to World to get ASCII diagram.
    if (auto* worldImpl = dynamic_cast<const World*>(world)) {
        logInitialTestState(worldImpl, test_description);
    }
}

void VisualTestBase::logWorldState(const World* world, const std::string& context) {
    if (!world) return;
    
    spdlog::debug("=== World State: {} ===", context);
    double totalMass = 0.0;
    const double PRESSURE_LOG_THRESHOLD = 0.0001;  // Very small threshold for pressure logging.
    const double VELOCITY_LOG_THRESHOLD = 0.0001;  // Threshold for velocity logging.
    
    for (uint32_t y = 0; y < world->getHeight(); y++) {
        for (uint32_t x = 0; x < world->getWidth(); x++) {
            const Cell& cell = world->at(x, y);
            if (cell.getFillRatio() > 0.001) {  // Only log cells with meaningful mass.
                // Build the log message dynamically based on what's present.
                std::stringstream ss;
                ss << "  Cell(" << x << "," << y << ") - "
                   << "Material: " << getMaterialName(cell.getMaterialType())
                   << ", Fill: " << std::fixed << std::setprecision(6) << cell.getFillRatio();
                
                // Only log velocity if it's non-zero.
                const Vector2d& velocity = cell.getVelocity();
                if (std::abs(velocity.x) > VELOCITY_LOG_THRESHOLD || 
                    std::abs(velocity.y) > VELOCITY_LOG_THRESHOLD) {
                    ss << ", Velocity: (" << std::fixed << std::setprecision(3) 
                       << velocity.x << "," << velocity.y << ")";
                }
                
                // Always log COM.
                ss << ", COM: (" << std::fixed << std::setprecision(3) 
                   << cell.getCOM().x << "," << cell.getCOM().y << ")";
                
                // Check if cell has any significant pressure components.
                double hydrostaticPressure = cell.getHydrostaticPressure();
                double dynamicPressure = cell.getDynamicPressure();
                double debugPressure = cell.getDynamicPressure();
                const Vector2d& gradient = cell.getPressureGradient();
                
                bool hasPressure = (hydrostaticPressure > PRESSURE_LOG_THRESHOLD || 
                                   dynamicPressure > PRESSURE_LOG_THRESHOLD ||
                                   debugPressure > PRESSURE_LOG_THRESHOLD ||
                                   gradient.magnitude() > PRESSURE_LOG_THRESHOLD);
                
                if (hasPressure) {
                    // Log pressure components separately.
                    if (hydrostaticPressure > PRESSURE_LOG_THRESHOLD) {
                        ss << ", HydroP: " << std::fixed << std::setprecision(6) << hydrostaticPressure;
                    }
                    if (dynamicPressure > PRESSURE_LOG_THRESHOLD) {
                        ss << ", DynP: " << std::fixed << std::setprecision(6) << dynamicPressure;
                    }
                    if (debugPressure > PRESSURE_LOG_THRESHOLD) {
                        ss << ", DebugP: " << std::fixed << std::setprecision(6) << debugPressure;
                    }
                    if (gradient.magnitude() > PRESSURE_LOG_THRESHOLD) {
                        ss << ", Gradient: (" << std::fixed << std::setprecision(6) 
                           << gradient.x << "," << gradient.y << ")";
                    }
                }
                
                spdlog::debug(ss.str());
                totalMass += cell.getFillRatio();
            }
        }
    }
    spdlog::debug("  Total mass in world: {:.6f}", totalMass);
}

void VisualTestBase::updateDisplay(World* world, const std::string& status) {
    if (!status.empty()) {
        spdlog::info("[STATUS] {}", status);
    }
    
    if (visual_mode_ && world) {
        auto& coordinator = VisualTestCoordinator::getInstance();
        coordinator.postTaskSync([this, world, status] {
            if (ui_) {
                world->draw(*ui_->getDrawArea());
                if (!status.empty()) {
                    ui_->updateButtonStatus(status);
                }
            }
        });
    }
}


void VisualTestBase::showInitialState(World* world, const std::string& description) {
    if (!world) return;
    
    // Log initial state with ASCII diagram.
    logInitialTestState(world, description);
    
    if (visual_mode_) {
        // Only disable restart if not already in a restart loop.
        // This preserves restart functionality when called from runRestartableTest.
        if (!restart_enabled_) {
            disableTestRestart();
        }
        
        // Update visual display.
        updateDisplay(world, "Initial state: " + description);
        
        // Wait for user to start.
        waitForStart();
        
        // If test was skipped, don't run it.
        if (isTestSkipped()) {
            spdlog::info("[TEST] Test skipped by user");
            return;
        }
    }
}

void VisualTestBase::showInitialStateWithStep(World* world, const std::string& description) {
    if (!world) return;
    
    // Log initial state with ASCII diagram.
    logInitialTestState(world, description);
    
    if (visual_mode_) {
        // Only disable restart if not already in a restart loop.
        // This preserves restart functionality when called from runRestartableTest.
        if (!restart_enabled_) {
            disableTestRestart();
        }
        
        // Update visual display.
        updateDisplay(world, "Initial state: " + description);
        
        // Wait for user to choose Start or Step.
        TestAction action = waitForStartOrStep();
        
        // Check if test was skipped.
        if (action == TestAction::NEXT || isTestSkipped()) {
            spdlog::info("[TEST] Test skipped by user");
            return;
        }
        
        // Store the chosen action for later use in stepSimulation.
        if (action == TestAction::STEP) {
            // Enable step mode in UI.
            auto& coordinator = VisualTestCoordinator::getInstance();
            coordinator.postTaskSync([this] {
                ui_->setStepMode(true);
            });
        }
    }
}

void VisualTestBase::stepSimulation(World* world, int steps, const std::string& stepDescription) {
    if (!world) return;
    
    if (visual_mode_) {
        auto& coordinator = VisualTestCoordinator::getInstance();
        
        for (int i = 0; i < steps; ++i) {
            // In step mode, wait for user input BEFORE advancing.
            if (ui_ && ui_->isStepModeEnabled()) {
                // Update status to show what's about to happen.
                std::string preStepStatus = stepDescription.empty() ? 
                    "Ready for step " + std::to_string(i + 1) + "/" + std::to_string(steps) + " - press Step" :
                    stepDescription + " [" + std::to_string(i + 1) + "/" + std::to_string(steps) + "] - press Step";
                
                coordinator.postTaskSync([this, preStepStatus] {
                    if (ui_) {
                        ui_->updateButtonStatus(preStepStatus);
                    }
                });
                
                TestAction action = waitForStep();
                
                // Handle the action.
                if (action == TestAction::START) {
                    // User wants to run continuously - exit step mode and run remaining steps.
                    spdlog::info("[TEST] Switching from step mode to continuous mode");
                    
                    // Run remaining steps continuously.
                    for (int j = i; j < steps; ++j) {
                        // Check if Next button was pressed during continuous run.
                        if (ui_ && ui_->next_pressed_.load()) {
                            spdlog::info("[TEST] Next button pressed during continuous run - skipping");
                            test_skipped_ = true;
                            coordinator.postTaskSync([this] {
                                ui_->disableNextButton();
                                ui_->updateButtonStatus("Test skipped");
                            });
                            return;
                        }
                        
                        world->advanceTime(0.016);
                        
                        std::string status = stepDescription.empty() ? 
                            "Step " + std::to_string(j + 1) + "/" + std::to_string(steps) :
                            stepDescription + " [" + std::to_string(j + 1) + "/" + std::to_string(steps) + "]";
                        
                        coordinator.postTaskSync([this, world, status] {
                            if (ui_) {
                                world->draw(*ui_->getDrawArea());
                                ui_->updateButtonStatus(status);
                            }
                        });
                        
                        pauseIfVisual(100);
                    }
                    
                    // Exit the outer loop since we've completed all steps.
                    break;
                } else if (action == TestAction::NEXT) {
                    // User wants to skip - exit immediately.
                    spdlog::info("[TEST] Skipping remaining steps");
                    return;
                }
                // Otherwise action == TestAction::STEP, continue stepping.
            }
            
            // Advance physics.
            world->advanceTime(0.016); // ~60 FPS timestep.
            
            // Update display with progress.
            std::string status = stepDescription.empty() ? 
                "Step " + std::to_string(i + 1) + "/" + std::to_string(steps) + " completed" :
                stepDescription + " [" + std::to_string(i + 1) + "/" + std::to_string(steps) + "]";
            
            coordinator.postTaskSync([this, world, status] {
                if (ui_) {
                    world->draw(*ui_->getDrawArea());
                    ui_->updateButtonStatus(status);
                }
            });
            
            // In continuous mode, add small pause to make steps visible.
            if (!ui_ || !ui_->isStepModeEnabled()) {
                // Check if Next button was pressed during continuous run.
                if (ui_ && ui_->next_pressed_.load()) {
                    spdlog::info("[TEST] Next button pressed during continuous run - skipping");
                    test_skipped_ = true;
                    coordinator.postTaskSync([this] {
                        ui_->disableNextButton();
                        ui_->updateButtonStatus("Test skipped");
                    });
                    return;
                }
            }
        }
        
        // Final status update and enable restart.
        if (ui_ && !test_skipped_) {
            std::string finalStatus = stepDescription.empty() ?
                "Completed " + std::to_string(steps) + " steps - Press Start to restart" :
                stepDescription + " - Complete - Press Start to restart";
            
            coordinator.postTaskSync([this, finalStatus] {
                ui_->updateButtonStatus(finalStatus);
                // Enable restart functionality.
                ui_->setRestartMode(true);
                ui_->enableStartButton();
                ui_->start_pressed_.store(false);
            });
            
            // Set restart enabled flag so the test can be rerun.
            restart_enabled_ = true;
        } else if (ui_) {
            // If test was skipped, just show the final status without restart option.
            std::string finalStatus = test_skipped_ ? "Test skipped" : 
                (stepDescription.empty() ? "Completed " + std::to_string(steps) + " steps" :
                 stepDescription + " - Complete");
            
            coordinator.postTaskSync([this, finalStatus] {
                ui_->updateButtonStatus(finalStatus);
            });
        }
    } else {
        // Non-visual mode: just advance time.
        double deltaTime = 0.016;
        for (int i = 0; i < steps; ++i) {
            world->advanceTime(deltaTime);
        }
    }
}

void VisualTestBase::runContinuousSimulation(World* world, int steps, const std::string& description) {
    if (!world) return;
    
    if (visual_mode_) {
        // If in step mode, use stepSimulation behavior instead.
        if (ui_ && ui_->isStepModeEnabled()) {
            stepSimulation(world, steps, description);
            return;
        }
        
        auto& coordinator = VisualTestCoordinator::getInstance();
        double deltaTime = 0.016; // ~60 FPS.
        
        for (int i = 0; i < steps; ++i) {
            // Check if Next button was pressed during continuous run.
            if (ui_ && ui_->next_pressed_.load()) {
                spdlog::info("[TEST] Next button pressed during continuous simulation - skipping");
                test_skipped_ = true;
                coordinator.postTaskSync([this] {
                    ui_->disableNextButton();
                    ui_->updateButtonStatus("Test skipped");
                });
                return;
            }
            
            // Advance physics.
            world->advanceTime(deltaTime);
            
            // Update display immediately without delay.
            std::string status = description.empty() ? 
                "Step " + std::to_string(i + 1) + "/" + std::to_string(steps) :
                description + " [" + std::to_string(i + 1) + "/" + std::to_string(steps) + "]";
            
            coordinator.postTaskSync([this, world, status] {
                if (ui_) {
                    world->draw(*ui_->getDrawArea());
                    ui_->updateButtonStatus(status);
                }
            });
            
            // Sleep for consistent frame rate (16ms = ~60 FPS).
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
        }
        
        // Final status update and enable restart.
        if (ui_ && !test_skipped_) {
            // Enable restart after successful completion.
            enableRestartAfterCompletion();
        } else if (ui_) {
            // If test was skipped, just show the final status.
            std::string finalStatus = "Test skipped";
            coordinator.postTaskSync([this, finalStatus] {
                ui_->updateButtonStatus(finalStatus);
            });
        }
    } else {
        // Non-visual mode: just advance time.
        double deltaTime = 0.016;
        for (int i = 0; i < steps; ++i) {
            world->advanceTime(deltaTime);
        }
    }
}

// Utility function for backward compatibility.
bool isVisualModeEnabled() {
    return VisualTestCoordinator::getInstance().isVisualModeEnabled();
} 
