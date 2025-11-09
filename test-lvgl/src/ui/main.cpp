#include "state-machine/StateMachine.h"
// TODO: Re-enable when integrating UI components:
// #include "SimulatorUI.h"
#include "core/World.h"

#include "args.hxx"
#include "lib/driver_backends.h"
#include "lib/simulator_loop.h"
#include "lib/simulator_settings.h"
#include "lib/simulator_util.h"

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

/* Global simulator settings, defined in lv_linux_backend.c */
extern simulator_settings_t settings;

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
        auto logger = std::make_shared<spdlog::logger>("ui", sinks.begin(), sinks.end());

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
        parser,
        "backend",
        "Select display backend (wayland, x11, fbdev, sdl)",
        { 'b', "backend" },
        "wayland");
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
    args::ValueFlag<std::string> server_host(
        parser,
        "server",
        "Auto-connect to DSSM server (format: host:port, e.g. localhost:8080)",
        { 'c', "connect" });

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
        // No backend specified, use wayland as default.
        selected_backend = "wayland";
    }

    // Apply settings from command line arguments.
    if (window_width) settings.window_width = args::get(window_width);
    if (window_height) settings.window_height = args::get(window_height);
    if (max_steps) settings.max_steps = args::get(max_steps);

    /* Initialize LVGL. */
    lv_init();

    /* Initialize the configured backend. */
    const char* backend_name = selected_backend.empty() ? nullptr : selected_backend.c_str();
    if (driver_backends_init_backend(backend_name) == -1) {
        die("Failed to initialize display backend");
    }

    spdlog::info("Starting with new UI state machine (UISM)");

    // Create the UI state machine with display.
    auto stateMachine = std::make_unique<DirtSim::Ui::StateMachine>(lv_disp_get_default());

    spdlog::info("UI state machine created, state: {}", stateMachine->getCurrentStateName());

    // Send init complete event to start state machine flow.
    stateMachine->queueEvent(DirtSim::Ui::InitCompleteEvent{});

    // Auto-connect to DSSM server (default: localhost:8080).
    if (server_host) {
        std::string hostPort = args::get(server_host);
        size_t colonPos = hostPort.find(':');
        if (colonPos != std::string::npos) {
            std::string host = hostPort.substr(0, colonPos);
            uint16_t port = static_cast<uint16_t>(std::stoi(hostPort.substr(colonPos + 1)));
            spdlog::info("Auto-connecting to DSSM server at {}:{}", host, port);
            stateMachine->queueEvent(DirtSim::Ui::ConnectToServerCommand{ host, port });
        }
        else {
            spdlog::error("Invalid server format (use host:port): {}", hostPort);
        }
    }
    else {
        // No server specified, connect to localhost:8080 by default.
        spdlog::info("Auto-connecting to DSSM server at localhost:8080 (default)");
        stateMachine->queueEvent(DirtSim::Ui::ConnectToServerCommand{ "localhost", 8080 });
    }

    spdlog::info("Entering backend run loop (will process events and LVGL)");

    // Enter the run loop with the state machine.
    // This integrates state machine event processing with LVGL event loop.
    driver_backends_run_loop(*stateMachine);

    spdlog::info("Backend run loop exited");
    spdlog::info("Application shutting down cleanly");

    return 0;
}
