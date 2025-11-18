/**
 * @file x11.c
 *
 * The backend for the X11 windowing system
 *
 * Based on the original file from the repository.
 *
 * Move to a separate file
 * 2025 EDGEMTech Ltd.
 *
 * Author: EDGEMTech Ltd, Erik Tagirov (erik.tagirov@edgemtech.ch)
 *
 */

/*********************
 *      INCLUDES
 *********************/

#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>

#include "lvgl/lvgl.h"
#include "ui/state-machine/StateMachine.h"
#include <spdlog/spdlog.h>

#if LV_USE_X11
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
static lv_display_t* init_x11(void);
static void run_loop_x11(DirtSim::Ui::StateMachine& sm);

/**********************
 *  STATIC VARIABLES
 **********************/
static const char* backend_name = "X11";

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
int backend_init_x11(backend_t* backend)
{
    LV_ASSERT_NULL(backend);
    backend->handle->display = static_cast<display_backend_t*>(malloc(sizeof(display_backend_t)));
    LV_ASSERT_NULL(backend->handle->display);

    backend->name = backend_name;
    backend->handle->display->init_display = init_x11;
    backend->handle->display->run_loop = run_loop_x11;
    backend->type = BACKEND_DISPLAY;

    return 0;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

/**
 * Initialize the X11 display driver
 *
 * @return the LVGL display
 */
static lv_display_t* init_x11(void)
{
    lv_display_t* disp;
    LV_IMG_DECLARE(mouse_cursor_icon);

    disp = lv_x11_window_create("Dirt Sim", settings.window_width, settings.window_height);

    disp = lv_display_get_default();

    if (disp == NULL) {
        return NULL;
    }

    lv_x11_inputs_create(disp, &mouse_cursor_icon);

    return disp;
}

/**
 * The run loop of the X11 driver
 */
void run_loop_x11(DirtSim::Ui::StateMachine& sm)
{
    uint32_t idle_time;

    /* Handle LVGL tasks. */
    while (!sm.shouldExit()) {
        // Process UI state machine events.
        sm.processEvents();

        // Update background animations (event-driven, no timer).
        sm.updateAnimations();

        /* Returns the time to the next timer execution. */
        idle_time = lv_timer_handler();

        // TODO: Get frame limiting from settings or config.
        bool frame_limiting_enabled = true;
        if (frame_limiting_enabled) {
            usleep(idle_time * 1000);
        }
    }

    // Process any final UI updates.
    for (int i = 0; i < 3; ++i) {
        lv_timer_handler();
        usleep(10000); // 10ms.
    }

    // TODO: Take exit screenshot.
    // SimulatorUI::takeExitScreenshot();
}

#endif /*#if LV_USE_X11. */
