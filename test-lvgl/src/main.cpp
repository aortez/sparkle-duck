#include "CrashDumpHandler.h"
#include "DirtSimStateMachine.h"
#include "Event.h"
#include "SimulationManager.h"
#include "SimulatorUI.h"
#include "World.h"
#include "WorldFactory.h"
#include "args.hxx"
#include "src/lib/driver_backends.h"
#include "src/lib/simulator_loop.h"
#include "src/lib/simulator_settings.h"
#include "src/lib/simulator_util.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <math.h>
#include <memory>
#include <stdio.h>
#include <string.h>
#include <thread>
#include <unistd.h>
#include <vector>

#include "lvgl/lvgl.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/spdlog.h"

/* Internal functions */
static void print_lvgl_version();

/* contains the name of the selected backend if user
 * has specified one on the command line */
static std::string selected_backend;

/* contains the selected world type */
static WorldType selected_world_type = WorldType::RulesB;

/* flag to use the new event-driven system */
static bool use_event_system = false;

/* flag to use push-based UI updates */
static bool use_push_updates = false;

/* Global simulator settings, defined in lv_linux_backend.c */
extern simulator_settings_t settings;

// Global references for the loop.
static SimulationManager* manager_ptr = nullptr;

// FPS tracking variables.
uint32_t frame_count = 0;     // Define frame counter.
uint32_t last_fps_update = 0; // Define last FPS update time.
uint32_t fps = 0;             // Define FPS value.

static void print_lvgl_version()
{
    fprintf(
        stdout,
        "%d.%d.%d-%s\n",
        LVGL_VERSION_MAJOR,
        LVGL_VERSION_MINOR,
        LVGL_VERSION_PATCH,
        LVGL_VERSION_INFO);
}

int main(int argc, char** argv)
{
    // Set up file and console logging.
    try {
        // Create console sink with colors.
        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        console_sink->set_level(spdlog::level::info);

        // Create basic file sink (overwrites each run).
        auto file_sink =
            std::make_shared<spdlog::sinks::basic_file_sink_mt>("sparkle-duck.log", true);
        file_sink->set_level(spdlog::level::info);

        // Create logger with both sinks.
        std::vector<spdlog::sink_ptr> sinks{ console_sink, file_sink };
        auto logger = std::make_shared<spdlog::logger>("sparkle-duck", sinks.begin(), sinks.end());

        // Set as default logger.
        spdlog::set_default_logger(logger);
        spdlog::flush_every(std::chrono::seconds(1));
    }
    catch (const spdlog::spdlog_ex& ex) {
        std::cout << "Log initialization failed: " << ex.what() << std::endl;
        return 1;
    }

    spdlog::info("🦆 Sparkle Duck Dirt Simulator starting up! 🦆");

    driver_backends_register();

    args::ArgumentParser parser(
        "Sparkle Duck - A cell-based multi-material physics simulation",
        "Default window size (1200x1200) provides a square window with comfortable space for the "
        "UI.");

    args::HelpFlag help(parser, "help", "Display this help menu", { 'h', "help" });
    args::Flag version(parser, "version", "Print LVGL version", { 'V', "version" });
    args::Flag list_backends(
        parser, "list-backends", "List supported backends", { 'B', "list-backends" });
    args::ValueFlag<std::string> backend(
        parser, "backend", "Select display backend (wayland, x11, fbdev, sdl)", { 'b', "backend" });
    args::ValueFlag<int> window_width(
        parser, "width", "Set window width (default: 1200)", { 'W', "width" }, 1200);
    args::ValueFlag<int> window_height(
        parser, "height", "Set window height (default: 1200)", { 'H', "height" }, 1200);
    args::ValueFlag<int> max_steps(
        parser,
        "steps",
        "Set maximum number of simulation steps (0 = unlimited)",
        { 's', "steps" },
        0);
    args::ValueFlag<std::string> world_type(
        parser,
        "world",
        "Select physics system: rulesA (mixed materials) or rulesB (pure materials, default)",
        { 'w', "world" },
        "rulesB");
    args::Flag event_system(
        parser,
        "event-system",
        "Use the new event-driven state machine (experimental)",
        { "event-system" });
    args::Flag push_updates(
        parser,
        "push-updates",
        "Enable push-based UI updates for thread safety (experimental)",
        { 'p', "push-updates" });

    // Default values from environment if set.
    settings.window_width = atoi(getenv("LV_SIM_WINDOW_WIDTH") ?: "1200");
    settings.window_height = atoi(getenv("LV_SIM_WINDOW_HEIGHT") ?: "1200");
    settings.max_steps = 0;

    try {
        parser.ParseCLI(argc, argv);
    }
    catch (const args::Help&) {
        std::cout << parser;
        return 0;
    }
    catch (const args::ParseError& e) {
        std::cerr << e.what() << std::endl;
        std::cerr << parser;
        return 1;
    }
    catch (const args::ValidationError& e) {
        std::cerr << e.what() << std::endl;
        std::cerr << parser;
        return 1;
    }

    // Process parsed arguments.
    if (version) {
        print_lvgl_version();
        return 0;
    }

    if (list_backends) {
        driver_backends_print_supported();
        return 0;
    }

    if (backend) {
        selected_backend = args::get(backend);
        if (driver_backends_is_supported(selected_backend.c_str()) == 0) {
            std::cerr << "Error: no such backend: " << selected_backend << std::endl;
            return 1;
        }
    }
    else {
        // No backend specified, use default (empty string will auto-select).
        selected_backend = "";
    }

    // Apply settings from command line arguments.
    if (window_width) settings.window_width = args::get(window_width);
    if (window_height) settings.window_height = args::get(window_height);
    if (max_steps) settings.max_steps = args::get(max_steps);

    if (world_type) {
        try {
            selected_world_type = parseWorldType(args::get(world_type));
        }
        catch (const std::runtime_error& e) {
            std::cerr << "Error: " << e.what() << std::endl;
            return 1;
        }
    }

    if (event_system) {
        use_event_system = true;
        spdlog::info("Event-driven state machine enabled (experimental)");
    }

    if (push_updates) {
        use_push_updates = true;
        spdlog::info("Push-based UI updates enabled (experimental)");
        if (!event_system) {
            spdlog::warn(
                "Push-based UI updates require --event-system flag, enabling it automatically");
            use_event_system = true;
        }
    }

    /* Initialize LVGL. */
    lv_init();

    /* Initialize the configured backend. */
    const char* backend_name = selected_backend.empty() ? nullptr : selected_backend.c_str();
    if (driver_backends_init_backend(backend_name) == -1) {
        die("Failed to initialize display backend");
    }

    // Calculate grid size based on cell size and drawing area.
    // (One fewer than would fit perfectly).
    const int grid_width = (850 / Cell::WIDTH) - 1;
    const int grid_height = (850 / Cell::HEIGHT) - 1;

    if (use_event_system) {
        spdlog::info("Starting with event-driven state machine");

        // Create the state machine with display.
        auto stateMachine = std::make_unique<DirtSim::DirtSimStateMachine>(lv_disp_get_default());

        // Enable push-based UI updates if requested.
        if (use_push_updates) {
            stateMachine->getSharedState().enablePushUpdates(true);
            spdlog::info("Push-based UI updates activated in state machine");
        }

        // Start the state machine's event processing thread.
        std::thread stateMachineThread([&stateMachine]() {
            spdlog::info("Starting state machine event processing thread");
            stateMachine->mainLoopRun();
            spdlog::info("State machine event processing thread exiting");
        });

        // Initialize the state machine - transition from Startup -> MainMenu -> SimRunning.
        stateMachine->getEventRouter().routeEvent(InitCompleteEvent{});

        // Skip the main menu and go directly to simulation for now.
        // TODO: In the future, show the main menu and let user click start.
        stateMachine->getEventRouter().routeEvent(StartSimulationCommand{});

        // Wait a bit for events to be processed by the state machine thread.
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        // Get the SimulationManager from the state machine.
        auto* simManager = stateMachine->getSimulationManager();
        if (!simManager) {
            spdlog::error("Failed to get SimulationManager from state machine");
            spdlog::warn("Falling back to traditional SimulationManager");
            use_event_system = false;
        }
        else {
            manager_ptr = simManager;

            spdlog::info(
                "Using SimulationManager from event system ({}x{} grid)",
                simManager->getWidth(),
                simManager->getHeight());

            // Install crash dump handler.
            CrashDumpHandler::install(manager_ptr);
            spdlog::info("Crash dump handler installed - assertions will generate JSON dumps");

            // Enter the run loop with the state machine's SimulationManager.
            driver_backends_run_loop(*simManager);

            // Signal the state machine to exit.
            stateMachine->getSharedState().setShouldExit(true);

            // Wait for the state machine thread to finish.
            if (stateMachineThread.joinable()) {
                spdlog::info("Waiting for state machine thread to finish...");
                stateMachineThread.join();
            }

            // Cleanup.
            CrashDumpHandler::uninstall();

            // State machine will clean up when it goes out of scope.
            return 0;
        }
    }

    if (!use_event_system) {
        // Traditional path with SimulationManager.
        auto manager = std::make_unique<SimulationManager>(
            selected_world_type, grid_width, grid_height, lv_scr_act(), nullptr);
        manager_ptr = manager.get();

        spdlog::info(
            "Created {} physics system ({}x{} grid)",
            getWorldTypeName(selected_world_type),
            grid_width,
            grid_height);

        // Install crash dump handler.
        CrashDumpHandler::install(manager_ptr);
        spdlog::info("Crash dump handler installed - assertions will generate JSON dumps");

        // Initialize the simulation.
        manager->initialize();

        // Enter the run loop, using the selected backend.
        driver_backends_run_loop(*manager);

        // Cleanup crash dump handler.
        CrashDumpHandler::uninstall();
    }

    spdlog::info("Application shutting down cleanly");

    return 0;
}
