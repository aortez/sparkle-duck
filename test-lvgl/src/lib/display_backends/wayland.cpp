/**
 * @file wayland.c
 *
 * The wayland backend
 *
 * Based on the original file from the repository
 *
 * - Move to a seperate file
 *   2025 EDGEMTech Ltd.
 *
 * Author: EDGEMTech Ltd, Erik Tagirov (erik.tagirov@edgemtech.ch)
 *
 */

/*********************
 *      INCLUDES
 *********************/
#include "stdio.h"
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>

#include "../World.h"
#include "lvgl/lvgl.h"
#include "simulator_loop.h"

#if LV_USE_WAYLAND
#include "../backends.h"
#include "../simulator_settings.h"
#include "../simulator_util.h"

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/
static lv_display_t* init_wayland(void);
static void run_loop_wayland(World& world);

/**********************
 *  STATIC VARIABLES
 **********************/
static char* backend_name = "WAYLAND";

/**********************
 *  EXTERNAL VARIABLES
 **********************/
extern simulator_settings_t settings;

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

/**
 * Register the backend
 *
 * @param backend the backend descriptor
 * @description configures the descriptor
 */
int backend_init_wayland(backend_t* backend)
{
    LV_ASSERT_NULL(backend);
    backend->handle->display = static_cast<display_backend_t*>(malloc(sizeof(display_backend_t)));
    LV_ASSERT_NULL(backend->handle->display);

    backend->handle->display->init_display = init_wayland;
    backend->handle->display->run_loop = run_loop_wayland;
    backend->name = backend_name;
    backend->type = BACKEND_DISPLAY;

    return 0;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

/**
 * Initialize the Wayland display driver
 *
 * @return the LVGL display
 */
static lv_display_t* init_wayland(void)
{
    lv_display_t* disp;
    lv_group_t* g;

    disp =
        lv_wayland_window_create(settings.window_width, settings.window_height, "Dirt Sim", NULL);

    if (disp == NULL) {
        die("Failed to initialize Wayland backend\n");
    }

    if (settings.fullscreen) {
        lv_wayland_window_set_fullscreen(disp, true);
    }
    else if (settings.maximize) {
        lv_wayland_window_set_maximized(disp, true);
    }

    g = lv_group_create();
    lv_group_set_default(g);
    lv_indev_set_group(lv_wayland_get_keyboard(disp), g);
    lv_indev_set_group(lv_wayland_get_pointeraxis(disp), g);

    return disp;
}

/**
 *
 * The run loop of the DRM driver
 *
 * @note Currently, the wayland driver calls lv_timer_handler internally.
 * The wayland driver needs to be re-written to match the other backends.
 */
static void run_loop_wayland(World& world)
{
    SimulatorLoop::LoopState state;
    SimulatorLoop::initState(state);
    
    // Set max_steps from global settings
    state.max_steps = settings.max_steps;

    bool completed;

    /* Handle LVGL tasks */
    while (state.is_running) {
        // Process one frame of simulation.
        SimulatorLoop::processFrame(world, state, 8);

        // Exit if step limit reached
        if (!state.is_running) {
            break;
        }

        // Mass label is now updated automatically by the World through its UI

        completed = lv_wayland_timer_handler();

        if (completed) {
            /* wait only if the cycle was completed */
            usleep(LV_DEF_REFR_PERIOD * 1000);
        }

        /* Run until the last window closes */
        if (!lv_wayland_window_is_open(NULL)) {
            break;
        }
    }
}

#endif /*#if LV_USE_WAYLAND*/
