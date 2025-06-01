#include "World.h"
#include "src/lib/driver_backends.h"
#include "src/lib/simulator_settings.h"
#include "src/lib/simulator_util.h"
#include "src/lib/simulator_loop.h"

#include <string.h>
#include <unistd.h>
#include <math.h>
#include <stdio.h>  // For fprintf and stdout

#include "lvgl/lvgl.h"
#include "lvgl/src/misc/lv_event.h"  // For LV_EVENT_* definitions
#include "lvgl/src/core/lv_obj_event.h"  // For lv_event_code_t and other event types

/* Internal functions */
static void configure_simulator(int argc, char** argv);
static void print_lvgl_version();
static void print_usage();

/* contains the name of the selected backend if user
 * has specified one on the command line */
static const char* selected_backend;

/* Global simulator settings, defined in lv_linux_backend.c */
extern simulator_settings_t settings;

// Static variables for callbacks
static World* world_ptr = nullptr;
static double timescale = 1.0;              // Default to 100% speed.
static bool is_paused = false;              // Track pause state.
static lv_obj_t* pause_label_ptr = nullptr; // Store pause label pointer.
lv_obj_t* mass_label_ptr = nullptr;         // Store mass label pointer.
lv_obj_t* fps_label_ptr = nullptr;          // Store FPS label pointer.

// FPS tracking variables
uint32_t frame_count = 0;      // Define frame counter
uint32_t last_fps_update = 0;  // Define last FPS update time
uint32_t fps = 0;              // Define FPS value

// Static callback for pause button.
static void pause_btn_event_cb(lv_event_t* e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        is_paused = !is_paused;
        lv_label_set_text(pause_label_ptr, is_paused ? "Resume" : "Pause");
        if (world_ptr) {
            world_ptr->setTimescale(is_paused ? 0.0 : timescale);
        }
    }
}

// Callback for timescale slider
static void timescale_slider_event_cb(lv_event_t* e)
{
    lv_obj_t* slider = static_cast<lv_obj_t*>(lv_event_get_target(e));
    lv_obj_t* timescale_value_label = static_cast<lv_obj_t*>(lv_event_get_user_data(e));
    if (lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED) {
        int32_t value = lv_slider_get_value(slider);
        // Logarithmic scale: timescale = 10^((value-50)/50)
        timescale = pow(10.0, (value - 50) / 50.0);
        if (world_ptr) {
            world_ptr->setTimescale(timescale);
        }
        // Update the timescale value label
        char buf[16];
        snprintf(buf, sizeof(buf), "%.2fx", timescale);
        lv_label_set_text(timescale_value_label, buf);
    }
}

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

    selected_backend = nullptr;
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

    // Create a drawing area.
    lv_obj_t* draw_area = lv_obj_create(lv_scr_act());
    lv_obj_set_size(draw_area, 800, 800);
    lv_obj_align(draw_area, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_set_style_pad_all(draw_area, 0, 0);  // Ensure no padding affects positioning

    // Create a world.
    World world(25, 25, draw_area);
    world.fillWithDirt();
    world.makeWalls();

    // Add click event handler to draw area.
    lv_obj_add_event_cb(draw_area, [](lv_event_t* e) {
        lv_event_code_t code = lv_event_get_code(e);
        World* world_ptr = static_cast<World*>(lv_event_get_user_data(e));
        
        lv_point_t point;
        lv_indev_get_point(lv_indev_get_act(), &point);
        
        // Get draw area coordinates
        lv_area_t area;
        lv_obj_get_coords(static_cast<lv_obj_t*>(lv_event_get_target(e)), &area);
        
        // Adjust point coordinates relative to draw area
        point.x -= area.x1;
        point.y -= area.y1;
        
        if (code == LV_EVENT_PRESSED) {
            // Add water at the clicked location
            world_ptr->addWaterAtPixel(point.x, point.y);
            // Then start dragging from that location
            world_ptr->startDragging(point.x, point.y);
            world_ptr->updateCursorForce(point.x, point.y, true);
        }
        else if (code == LV_EVENT_PRESSING) {
            world_ptr->updateDrag(point.x, point.y);
            world_ptr->updateCursorForce(point.x, point.y, true);
        }
        else if (code == LV_EVENT_RELEASED) {
            world_ptr->endDragging(point.x, point.y);
            world_ptr->clearCursorForce();
        }
    }, LV_EVENT_ALL, &world);

    // Create simulation loop state and event context
    static SimulatorLoop::LoopState sim_state;

    // Control column width
    const int control_width = 200;

    // Create mass label (move to left side, top of control column)
    lv_obj_t* mass_label = lv_label_create(lv_scr_act());
    lv_label_set_text(mass_label, "Total Mass: 0.00");
    lv_obj_align(mass_label, LV_ALIGN_TOP_LEFT, 820, 10);
    mass_label_ptr = mass_label; // Store pointer for updates.

    // Create FPS label (move to left side, below mass label)
    lv_obj_t* fps_label = lv_label_create(lv_scr_act());
    lv_label_set_text(fps_label, "FPS: 0");
    lv_obj_align(fps_label, LV_ALIGN_TOP_LEFT, 820, 40);
    fps_label_ptr = fps_label; // Store pointer for updates.

    // Controls start at x=800+control_width-10 (right edge of control column)
    int control_x = 800 + control_width - 10;

    // Create reset button.
    lv_obj_t* reset_btn = lv_btn_create(lv_scr_act());
    lv_obj_set_size(reset_btn, control_width, 50);
    lv_obj_align(reset_btn, LV_ALIGN_TOP_RIGHT, -10, 10);
    lv_obj_t* reset_label = lv_label_create(reset_btn);
    lv_label_set_text(reset_label, "Reset");
    lv_obj_center(reset_label);

    // Create pause button.
    lv_obj_t* pause_btn = lv_btn_create(lv_scr_act());
    lv_obj_set_size(pause_btn, control_width, 50);
    lv_obj_align(pause_btn, LV_ALIGN_TOP_RIGHT, -10, 70);
    lv_obj_t* pause_label = lv_label_create(pause_btn);
    lv_label_set_text(pause_label, "Pause");
    lv_obj_center(pause_label);

    // Create debug toggle button (moved up)
    lv_obj_t* debug_btn = lv_btn_create(lv_scr_act());
    lv_obj_set_size(debug_btn, control_width, 50);
    lv_obj_align(debug_btn, LV_ALIGN_TOP_RIGHT, -10, 130);
    lv_obj_t* debug_label = lv_label_create(debug_btn);
    lv_label_set_text(debug_label, "Debug: Off");
    lv_obj_center(debug_label);
    lv_obj_add_event_cb(debug_btn, [](lv_event_t* e) {
        Cell::debugDraw = !Cell::debugDraw;
        const lv_obj_t* btn = static_cast<const lv_obj_t*>(lv_event_get_target(e));
        lv_obj_t* label = lv_obj_get_child(btn, 0);
        lv_label_set_text(label, Cell::debugDraw ? "Debug: On" : "Debug: Off");
    }, LV_EVENT_CLICKED, NULL);

    // Create cursor force toggle button (moved up)
    lv_obj_t* force_btn = lv_btn_create(lv_scr_act());
    lv_obj_set_size(force_btn, control_width, 50);
    lv_obj_align(force_btn, LV_ALIGN_TOP_RIGHT, -10, 190);
    lv_obj_t* force_label = lv_label_create(force_btn);
    lv_label_set_text(force_label, "Force: Off");
    lv_obj_center(force_label);

    // Create timescale slider (moved down)
    lv_obj_t* slider_label = lv_label_create(lv_scr_act());
    lv_label_set_text(slider_label, "Timescale");
    lv_obj_align(slider_label, LV_ALIGN_TOP_RIGHT, -10, 290);

    // Create label to show current timescale value
    lv_obj_t* timescale_value_label = lv_label_create(lv_scr_act());
    lv_label_set_text(timescale_value_label, "1.0x");
    lv_obj_align(timescale_value_label, LV_ALIGN_TOP_RIGHT, -120, 290);

    lv_obj_t* slider = lv_slider_create(lv_scr_act());
    lv_obj_set_size(slider, control_width, 10);
    lv_obj_align(slider, LV_ALIGN_TOP_RIGHT, -10, 310);
    lv_slider_set_range(slider, 0, 100);           // Log scale: 0.1x to 10x, 1.0x at 50.
    lv_slider_set_value(slider, 50, LV_ANIM_OFF);  // Start at 1.0x speed.

    // Create elasticity slider (moved down)
    lv_obj_t* elasticity_label = lv_label_create(lv_scr_act());
    lv_label_set_text(elasticity_label, "Elasticity");
    lv_obj_align(elasticity_label, LV_ALIGN_TOP_RIGHT, -10, 330);

    // Create label to show current elasticity value
    lv_obj_t* elasticity_value_label = lv_label_create(lv_scr_act());
    lv_label_set_text(elasticity_value_label, "0.8");
    lv_obj_align(elasticity_value_label, LV_ALIGN_TOP_RIGHT, -120, 330);

    lv_obj_t* elasticity_slider = lv_slider_create(lv_scr_act());
    lv_obj_set_size(elasticity_slider, control_width, 10);
    lv_obj_align(elasticity_slider, LV_ALIGN_TOP_RIGHT, -10, 350);
    lv_slider_set_range(elasticity_slider, 0, 200);           // Range [0, 2] with 0.01 steps
    lv_slider_set_value(elasticity_slider, 80, LV_ANIM_OFF);  // Start at 0.8

    // Create dirt fragmentation slider
    lv_obj_t* fragmentation_label = lv_label_create(lv_scr_act());
    lv_label_set_text(fragmentation_label, "Dirt Fragmentation");
    lv_obj_align(fragmentation_label, LV_ALIGN_TOP_RIGHT, -10, 370);

    // Create label to show current fragmentation value
    lv_obj_t* fragmentation_value_label = lv_label_create(lv_scr_act());
    lv_label_set_text(fragmentation_value_label, "0.0001");
    lv_obj_align(fragmentation_value_label, LV_ALIGN_TOP_RIGHT, -165, 370);

    lv_obj_t* fragmentation_slider = lv_slider_create(lv_scr_act());
    lv_obj_set_size(fragmentation_slider, control_width, 10);
    lv_obj_align(fragmentation_slider, LV_ALIGN_TOP_RIGHT, -10, 390);
    lv_slider_set_range(fragmentation_slider, 0, 100);           // Range [0, 100] for quadratic mapping
    lv_slider_set_value(fragmentation_slider, 20, LV_ANIM_OFF);  // Start at 0.1 (20^2/10000 = 0.04)

    // Create callback for fragmentation slider
    lv_obj_add_event_cb(
        fragmentation_slider,
        [](lv_event_t* e) {
            lv_obj_t* slider = static_cast<lv_obj_t*>(lv_event_get_target(e));
            lv_obj_t* fragmentation_value_label = static_cast<lv_obj_t*>(lv_event_get_user_data(e));
            if (lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED) {
                int32_t value = lv_slider_get_value(slider);
                // Quadratic scale: maps [0,100] to [0,0.5] with more precision at lower values
                double fragmentation = (value * value) / 20000.0;  // value^2 / 20000 gives [0,0.5]
                if (world_ptr) {
                    world_ptr->setDirtFragmentationFactor(fragmentation);
                }
                // Update the fragmentation value label
                char buf[16];
                snprintf(buf, sizeof(buf), "%.3f", fragmentation);
                lv_label_set_text(fragmentation_value_label, buf);
            }
        },
        LV_EVENT_ALL,
        fragmentation_value_label);

    // Create callback for elasticity slider
    lv_obj_add_event_cb(
        elasticity_slider,
        [](lv_event_t* e) {
            lv_obj_t* slider = static_cast<lv_obj_t*>(lv_event_get_target(e));
            lv_obj_t* elasticity_value_label = static_cast<lv_obj_t*>(lv_event_get_user_data(e));
            if (lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED) {
                int32_t value = lv_slider_get_value(slider);
                double elasticity = value / 100.0;  // Convert to [0, 2] range
                if (world_ptr) {
                    world_ptr->setElasticityFactor(elasticity);
                }
                // Update the elasticity value label
                char buf[16];
                snprintf(buf, sizeof(buf), "%.2f", elasticity);
                lv_label_set_text(elasticity_value_label, buf);
            }
        },
        LV_EVENT_ALL,
        elasticity_value_label);

    // Create quit button.
    lv_obj_t* quit_btn = lv_btn_create(lv_scr_act());
    lv_obj_set_size(quit_btn, control_width, 50);
    lv_obj_align(quit_btn, LV_ALIGN_BOTTOM_RIGHT, -10, -10); // Move to bottom right with margin

    // Make the button red.
    lv_obj_set_style_bg_color(quit_btn, lv_color_hex(0xFF0000), 0);

    lv_obj_t* quit_label = lv_label_create(quit_btn);
    lv_label_set_text(quit_label, "Quit");
    lv_obj_center(quit_label);

    // Create callback for quit button.
    lv_obj_add_event_cb(
        quit_btn,
        [](lv_event_t* e) {
            if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
                exit(0); // Exit the application.
            }
        },
        LV_EVENT_ALL,
        nullptr);

    // Create callback for timescale slider.
    lv_obj_add_event_cb(
        slider,
        timescale_slider_event_cb,
        LV_EVENT_ALL,
        timescale_value_label);

   // Create callback for reset button.
    lv_obj_add_event_cb(
        reset_btn,
        [](lv_event_t* e) {
            if (world_ptr) {
                world_ptr->reset();
            }
        },
        LV_EVENT_CLICKED,
        nullptr);

    // Store pause label pointer and set up callback.
    pause_label_ptr = pause_label;
    lv_obj_add_event_cb(pause_btn, pause_btn_event_cb, LV_EVENT_CLICKED, nullptr);

    // Init the world.
    world_ptr = &world; // Store pointer for callback.
    world.reset();

    // Enter the run loop, using the selected backend.
    driver_backends_run_loop(world);

    return 0;
}
