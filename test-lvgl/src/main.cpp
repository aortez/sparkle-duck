#include "SimulationManager.h"
#include "SimulatorUI.h"
#include "World.h"
#include "WorldFactory.h"
#include "CrashDumpHandler.h"
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
#include <unistd.h>
#include <vector>

#include "lvgl/lvgl.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/spdlog.h"

/* Internal functions */
static void configure_simulator(int argc, char** argv);
static void print_lvgl_version();
static void print_usage();

/* contains the name of the selected backend if user
 * has specified one on the command line */
static const char* selected_backend;

/* contains the selected world type */
static WorldType selected_world_type = WorldType::RulesB; // Default to RulesB per CLAUDE.md

/* Global simulator settings, defined in lv_linux_backend.c */
extern simulator_settings_t settings;

// Global references for the loop
static SimulationManager* manager_ptr = nullptr;

// FPS tracking variables
uint32_t frame_count = 0;     // Define frame counter
uint32_t last_fps_update = 0; // Define last FPS update time
uint32_t fps = 0;             // Define FPS value

/**
 * @brief Print LVGL version
 */
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

/**
 * @brief Print usage information
 */
static void print_usage(void)
{
    fprintf(
        stdout,
        "\nsparkle-duck [-V] [-B] [-b backend_name] [-W window_width] [-H "
        "window_height] [-s max_steps] [-w world_type]\n\n");
    fprintf(stdout, "-V print LVGL version\n");
    fprintf(stdout, "-B list supported backends\n");
    fprintf(stdout, "-b backend_name select display backend (wayland, x11, fbdev)\n");
    fprintf(stdout, "-W window_width set window width (default: 1200)\n");
    fprintf(stdout, "-H window_height set window height (default: 1200)\n");
    fprintf(stdout, "-s max_steps set maximum number of simulation steps (0 = unlimited)\n");
    fprintf(stdout, "-w world_type select physics system: rulesA (mixed materials) or rulesB (pure materials, default)\n");
    fprintf(
        stdout,
        "\nDefault window size (1200x1200) provides a square window with comfortable space for the "
        "UI.\n");
}

/**
 * @brief Configure simulator
 * @description process arguments recieved by the program to select
 * appropriate options
 * @param argc the count of arguments in argv
 * @param argv The arguments
 */
static void configure_simulator(int argc, char** argv)
{
    int opt = 0;

    selected_backend = nullptr;
    driver_backends_register();

    /* Default values - sized to fit UI content properly
     * UI Layout: 850px draw area + 200px controls + padding
     * 1200x1200 provides a square window with extra space for comfortable viewing */
    settings.window_width = atoi(getenv("LV_SIM_WINDOW_WIDTH") ?: "1200");
    settings.window_height = atoi(getenv("LV_SIM_WINDOW_HEIGHT") ?: "1200");
    settings.max_steps = 0; // Default to unlimited steps

    /* Parse the command-line options. */
    while ((opt = getopt(argc, argv, "b:fmW:H:s:w:BVh")) != -1) {
        switch (opt) {
            case 'h':
                print_usage();
                exit(EXIT_SUCCESS);
                break;
            case 'V':
                print_lvgl_version();
                exit(EXIT_SUCCESS);
                break;
            case 'B':
                driver_backends_print_supported();
                exit(EXIT_SUCCESS);
                break;
            case 'b':
                if (driver_backends_is_supported(optarg) == 0) {
                    die("error no such backend: %s\n", optarg);
                }
                selected_backend = strdup(optarg);
                break;
            case 'W':
                settings.window_width = atoi(optarg);
                break;
            case 'H':
                settings.window_height = atoi(optarg);
                break;
            case 's':
                settings.max_steps = atoi(optarg);
                break;
            case 'w':
                try {
                    selected_world_type = parseWorldType(optarg);
                } catch (const std::runtime_error& e) {
                    die("error: %s\n", e.what());
                }
                break;
            case ':':
                print_usage();
                die("Option -%c requires an argument.\n", optopt);
                break;
            case '?':
                print_usage();
                die("Unknown option -%c.\n", optopt);
        }
    }
}

// Add this at file scope (before main):

/**
 * @brief entry point
 * @description start a demo
 * @param argc the count of arguments in argv
 * @param argv The arguments
 */
int main(int argc, char** argv)
{
    // Set up file and console logging
    try {
        // Create console sink with colors
        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        console_sink->set_level(spdlog::level::info); // Only INFO and above to console

        // Create basic file sink (overwrites each run)
        auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(
            "sparkle-duck.log", true); // true = truncate (overwrite)
        file_sink->set_level(spdlog::level::trace); // Everything to file

        // Create logger with both sinks
        std::vector<spdlog::sink_ptr> sinks{ console_sink, file_sink };
        auto logger = std::make_shared<spdlog::logger>("sparkle-duck", sinks.begin(), sinks.end());

        // Set as default logger
        spdlog::set_default_logger(logger);
        spdlog::set_level(spdlog::level::debug);
        spdlog::flush_every(std::chrono::seconds(1)); // Flush every second
    }
    catch (const spdlog::spdlog_ex& ex) {
        std::cout << "Log initialization failed: " << ex.what() << std::endl;
        return 1;
    }

    spdlog::info("🦆 Sparkle Duck Dirt Simulator starting up! 🦆");
    spdlog::debug("Logging configured: console (INFO+) and file sparkle-duck.log (TRACE+)");

    configure_simulator(argc, argv);

    /* Initialize LVGL. */
    lv_init();

    /* Initialize the configured backend. */
    if (driver_backends_init_backend(selected_backend) == -1) {
        die("Failed to initialize display backend");
    }

    // Calculate grid size based on cell size and drawing area.
    // (One fewer than would fit perfectly).
    const int grid_width = (850 / Cell::WIDTH) - 1;
    const int grid_height = (850 / Cell::HEIGHT) - 1;

    // Create the simulation manager (which creates both UI and world)
    auto manager = std::make_unique<SimulationManager>(selected_world_type, grid_width, grid_height, lv_scr_act());
    manager_ptr = manager.get();
    
    spdlog::info("Created {} physics system ({}x{} grid)", 
                 getWorldTypeName(selected_world_type), grid_width, grid_height);

    // Install crash dump handler
    CrashDumpHandler::install(manager_ptr);
    spdlog::info("Crash dump handler installed - assertions will generate JSON dumps");

    // Initialize the simulation
    manager->initialize();

    // Enter the run loop, using the selected backend.
    driver_backends_run_loop(*manager);

    // Cleanup crash dump handler
    CrashDumpHandler::uninstall();
    spdlog::info("Application shutting down cleanly");

    return 0;
}
