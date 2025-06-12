#include "SimulatorUI.h"
#include "World.h"
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
#include "spdlog/sinks/rotating_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/spdlog.h"

/* Internal functions */
static void configure_simulator(int argc, char** argv);
static void print_lvgl_version();
static void print_usage();

/* contains the name of the selected backend if user
 * has specified one on the command line */
static const char* selected_backend;

/* Global simulator settings, defined in lv_linux_backend.c */
extern simulator_settings_t settings;

// Global references for the loop
static World* world_ptr = nullptr;
static SimulatorUI* ui_ptr = nullptr;

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
        "window_height] [-s max_steps]\n\n");
    fprintf(stdout, "-V print LVGL version\n");
    fprintf(stdout, "-B list supported backends\n");
    fprintf(stdout, "-b backend_name select display backend (wayland, x11, fbdev)\n");
    fprintf(stdout, "-W window_width set window width (default: 1200)\n");
    fprintf(stdout, "-H window_height set window height (default: 1200)\n");
    fprintf(stdout, "-s max_steps set maximum number of simulation steps (0 = unlimited)\n");
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
    while ((opt = getopt(argc, argv, "b:fmW:H:s:BVh")) != -1) {
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

        // Create rotating file sink (10MB files, max 3 files)
        auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            "sparkle-duck.log", 1024 * 1024 * 10, 3);
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

    spdlog::info("✨🦆 Sparkle Duck Dirt Simulator starting up! 🦆✨");
    spdlog::debug("Logging configured: console (INFO+) and file sparkle-duck.log (TRACE+)");

    configure_simulator(argc, argv);

    /* Initialize LVGL. */
    lv_init();

    /* Initialize the configured backend. */
    if (driver_backends_init_backend(selected_backend) == -1) {
        die("Failed to initialize display backend");
    }

    // Create UI first without a world.
    auto ui = std::make_unique<SimulatorUI>(lv_scr_act());
    ui_ptr = ui.get();
    ui->initialize();

    // Calculate grid size based on cell size and drawing area.
    // (One fewer than would fit perfectly).
    const int grid_width = (850 / Cell::WIDTH) - 1;
    const int grid_height = (850 / Cell::HEIGHT) - 1;

    // Create the world.
    auto world = std::make_unique<World>(grid_width, grid_height, ui->getDrawArea());

    // Connect the UI to the world.
    ui->setWorld(world.get());

    // Give the world ownership of the UI.
    world->setUI(std::move(ui));

    // Create simulation loop state and event context.
    static SimulatorLoop::LoopState sim_state;

    // Initialize the world.
    world->reset();

    // Enter the run loop, using the selected backend.
    driver_backends_run_loop(*world);

    return 0;
}
