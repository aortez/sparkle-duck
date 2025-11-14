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
#include <iostream>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>

#include "lvgl/lvgl.h"
#include "ui/state-machine/StateMachine.h"
#include <spdlog/spdlog.h>

#if LV_USE_WAYLAND
#include "ui/lib/backends.h"
#include "ui/lib/simulator_settings.h"
#include "ui/lib/simulator_util.h"

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  EXTERNAL VARIABLES
 **********************/
extern simulator_settings_t settings;

/**********************
 *  STATIC PROTOTYPES
 **********************/
static lv_display_t* init_wayland(void);
static void run_loop_wayland(DirtSim::Ui::StateMachine& sm);

/**********************
 *  STATIC VARIABLES
 **********************/
static const char* backend_name = "WAYLAND";

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

    disp = lv_wayland_window_create(
        settings.window_width, settings.window_height, const_cast<char*>("Dirt Sim"), NULL);

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
 * The run loop of the Wayland driver.
 */
static void run_loop_wayland(DirtSim::Ui::StateMachine& sm)
{
    bool completed;

    /* Handle LVGL tasks. */
    while (!sm.shouldExit()) {
        // Process UI state machine events.
        sm.processEvents();

        // Update background animations (event-driven, no timer).
        sm.updateAnimations();

        // Process LVGL timer events.
        completed = lv_wayland_timer_handler();

        if (completed) {
            /* Wait to avoid busy-looping and consuming 100% CPU. */
            usleep(1000);
        }

        /* Run until the last window closes. */
        if (!lv_wayland_window_is_open(NULL)) {
            spdlog::info("Wayland window closed, exiting");
            sm.setShouldExit(true);
            break;
        }
    }

    // Process any final UI updates.
    for (int i = 0; i < 3; ++i) {
        lv_wayland_timer_handler();
    }

    // TODO: Take exit screenshot.
    // SimulatorUI::takeExitScreenshot();
}

#endif /*#if LV_USE_WAYLAND. */
