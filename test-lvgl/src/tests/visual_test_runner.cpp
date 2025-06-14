#include "visual_test_runner.h"
#include <future>

// External global settings used by the backend system
extern simulator_settings_t settings;

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
        // Process LVGL timer handler first
        if (visual_mode_enabled_) {
            lv_wayland_timer_handler();
        }

        // Then process our tasks when LVGL is not rendering
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

        usleep(3000); // 3ms sleep to prevent busy-waiting
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
    VisualTestCoordinator::getInstance().initializeVisualMode();
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

    if (visual_mode_) {
        coordinator.postTaskSync([this, &coordinator] {
            lv_obj_clean(lv_scr_act());
            ui_ = std::make_unique<TestUI>(lv_scr_act(), current_test_name_);
            ui_->initialize();
        });
    }
}

void VisualTestBase::TearDown() {
    auto& coordinator = VisualTestCoordinator::getInstance();
    if (visual_mode_ && ui_) {
        coordinator.postTaskSync([this] {
            ui_.reset();
        });
    }
    std::cout << "=== Test " << current_test_name_ << " completed ===\n";
}

std::unique_ptr<World> VisualTestBase::createWorld(uint32_t width, uint32_t height) {
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

void VisualTestBase::runSimulation(World* world, int steps, const std::string& description) {
    if (visual_mode_) {
        std::cout << "  Running visual simulation: " << description << " (" << steps << " steps)\n";
        auto& coordinator = VisualTestCoordinator::getInstance();
        double deltaTime = 0.016;
        
        // Limit visual updates to reasonable framerate (every 3 physics steps = ~20 FPS)
        const int visual_update_interval = 3;
        
        for (int i = 0; i < steps; ++i) {
            world->advanceTime(deltaTime);
            
            // Only update visuals every few steps to reduce LVGL load
            if (i % visual_update_interval == 0 || i == steps - 1) {
                coordinator.postTask([world, this, description, i, steps] {
                    world->draw();
                    // Update label with current progress
                    std::string status = current_test_name_ + " - " + description +
                                         " [" + std::to_string(i + 1) + "/" + std::to_string(steps) + "]";
                    if(ui_) ui_->updateTestLabel(status);
                });
                
                // Give LVGL time to process the drawing
                std::this_thread::sleep_for(std::chrono::milliseconds(15));
            }
        }
        
        // Add a final sync to ensure all drawing is complete before test ends
        coordinator.postTaskSync([world] {
            world->draw();
        });
    } else {
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
