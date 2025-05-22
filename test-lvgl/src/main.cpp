#include "World.h"
#include "src/lib/driver_backends.h"
#include "src/lib/simulator_settings.h"
#include "src/lib/simulator_util.h"

#include <string.h>
#include <unistd.h>

#include "lvgl/lvgl.h"

/* Internal functions */
static void configure_simulator(int argc, char** argv);
static void print_lvgl_version(void);
static void print_usage(void);

/* contains the name of the selected backend if user
 * has specified one on the command line */
static char* selected_backend;

/* Global simulator settings, defined in lv_linux_backend.c */
extern simulator_settings_t settings;

// Static variables for callbacks
static World* world_ptr = nullptr;
static double timescale = 1.0;  // Default to 100% speed.
static bool is_paused = false;  // Track pause state.
static lv_obj_t* pause_label_ptr = nullptr;  // Store pause label pointer.

// Static callback for pause button.
static void pause_btn_event_cb(lv_event_t* e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        is_paused = !is_paused;
        lv_label_set_text(pause_label_ptr, is_paused ? "Resume" : "Pause");
        if (world_ptr) {
            world_ptr->setTimescale(is_paused ? 0.0 : timescale);
        }
    }
}

/**
 * @brief Print LVGL version
 */
static void print_lvgl_version(void)
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
        "\nlvglsim [-V] [-B] [-b backend_name] [-W window_width] [-H "
        "window_height]\n\n");
    fprintf(stdout, "-V print LVGL version\n");
    fprintf(stdout, "-B list supported backends\n");
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
    char* backend_name;

    selected_backend = NULL;
    driver_backends_register();

    /* Default values */
    settings.window_width = atoi(getenv("LV_SIM_WINDOW_WIDTH") ?: "800");
    settings.window_height = atoi(getenv("LV_SIM_WINDOW_HEIGHT") ?: "480");

    /* Parse the command-line options. */
    while ((opt = getopt(argc, argv, "b:fmW:H:BVh")) != -1) {
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

/**
 * @brief entry point
 * @description start a demo
 * @param argc the count of arguments in argv
 * @param argv The arguments
 */
int main(int argc, char** argv)
{
    configure_simulator(argc, argv);

    /* Initialize LVGL. */
    lv_init();

    /* Initialize the configured backend. */
    if (driver_backends_init_backend(selected_backend) == -1) {
        die("Failed to initialize display backend");
    }

    // Create drawing area.
    const uint32_t draw_area_width = 500;
    const uint32_t draw_area_height = 500;

    lv_obj_t* draw_area = lv_obj_create(lv_scr_act());
    lv_obj_set_size(draw_area, draw_area_width, draw_area_height);

    // Create reset button.
    lv_obj_t* reset_btn = lv_btn_create(lv_scr_act());
    lv_obj_set_size(reset_btn, 100, 50);
    lv_obj_align(reset_btn, LV_ALIGN_TOP_RIGHT, -10, 10);
    
    lv_obj_t* reset_label = lv_label_create(reset_btn);
    lv_label_set_text(reset_label, "Reset");
    lv_obj_center(reset_label);

    // Create pause button.
    lv_obj_t* pause_btn = lv_btn_create(lv_scr_act());
    lv_obj_set_size(pause_btn, 100, 50);
    lv_obj_align(pause_btn, LV_ALIGN_TOP_RIGHT, -10, 70);
    
    lv_obj_t* pause_label = lv_label_create(pause_btn);
    lv_label_set_text(pause_label, "Pause");
    lv_obj_center(pause_label);

    // Create timescale slider.
    lv_obj_t* slider_label = lv_label_create(lv_scr_act());
    lv_label_set_text(slider_label, "Timescale");
    lv_obj_align(slider_label, LV_ALIGN_TOP_RIGHT, -10, 130);

    lv_obj_t* slider = lv_slider_create(lv_scr_act());
    lv_obj_set_size(slider, 100, 10);
    lv_obj_align(slider, LV_ALIGN_TOP_RIGHT, -10, 150);
    lv_slider_set_range(slider, 5, 500);  // 5% to 500% speed.
    lv_slider_set_value(slider, 100, LV_ANIM_OFF);  // Start at 100%.

    // Create quit button.
    lv_obj_t* quit_btn = lv_btn_create(lv_scr_act());
    lv_obj_set_size(quit_btn, 100, 50);
    lv_obj_align(quit_btn, LV_ALIGN_TOP_RIGHT, -10, 170);
    
    // Make the button red.
    lv_obj_set_style_bg_color(quit_btn, lv_color_hex(0xFF0000), 0);
    
    lv_obj_t* quit_label = lv_label_create(quit_btn);
    lv_label_set_text(quit_label, "Quit");
    lv_obj_center(quit_label);

    // Create callback for quit button.
    lv_obj_add_event_cb(quit_btn, [](lv_event_t* e) {
        if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
            exit(0);  // Exit the application.
        }
    }, LV_EVENT_ALL, nullptr);

    // Create callback for timescale slider.
    lv_obj_add_event_cb(slider, [](lv_event_t* e) {
        lv_obj_t* slider = static_cast<lv_obj_t*>(lv_event_get_target(e));
        if (lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED) {
            int32_t value = lv_slider_get_value(slider);
            timescale = value / 100.0;  // Convert to 0.05 to 5.0 range.
            if (world_ptr) {
                world_ptr->setTimescale(timescale);
            }
        }
    }, LV_EVENT_ALL, nullptr);

    // Create callback for reset button.
    lv_obj_add_event_cb(reset_btn, [](lv_event_t* e) {
        if (world_ptr) {
            world_ptr->reset();
        }
    }, LV_EVENT_CLICKED, nullptr);

    // Store pause label pointer and set up callback.
    pause_label_ptr = pause_label;
    lv_obj_add_event_cb(pause_btn, pause_btn_event_cb, LV_EVENT_CLICKED, nullptr);

    // Init the world.
    World world(10, 10, draw_area);
    world_ptr = &world;  // Store pointer for callback.
    world.reset();

    // Enter the run loop, using the selected backend.
    driver_backends_run_loop(world);

    return 0;
}
