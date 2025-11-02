/**
 * @file fbdev.c
 *
 * Legacy framebuffer device
 *
 * Based on the original file from the repository
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

#include "../../DirtSimStateMachine.h"
#include "../../SimulatorUI.h"
#include "../WorldInterface.h"
#include "lvgl/lvgl.h"
#include "simulator_loop.h"
#if LV_USE_LINUX_FBDEV
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

static lv_display_t* init_fbdev(void);
static void run_loop_fbdev(DirtSim::DirtSimStateMachine& dsm);

/**********************
 *  STATIC VARIABLES
 **********************/

static const char* backend_name = "FBDEV";

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
int backend_init_fbdev(backend_t* backend)
{
    LV_ASSERT_NULL(backend);

    backend->handle->display = static_cast<display_backend_t*>(malloc(sizeof(display_backend_t)));
    LV_ASSERT_NULL(backend->handle->display);

    backend->handle->display->init_display = init_fbdev;
    backend->handle->display->run_loop = run_loop_fbdev;
    backend->name = backend_name;
    backend->type = BACKEND_DISPLAY;

    return 0;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

/**
 * Initialize the fbdev driver
 *
 * @return the LVGL display
 */
static lv_display_t* init_fbdev(void)
{
    const char* device = getenv_default("LV_LINUX_FBDEV_DEVICE", "/dev/fb0");
    lv_display_t* disp = lv_linux_fbdev_create();

    if (disp == NULL) {
        return NULL;
    }

    lv_linux_fbdev_set_file(disp, device);

    return disp;
}

/**
 * The run loop of the fbdev driver
 */
static void run_loop_fbdev(DirtSim::DirtSimStateMachine& dsm)
{
    // Initialize simulation loop state for step counting.
    SimulatorLoop::LoopState state;
    SimulatorLoop::initState(state);
    
    // Set max_steps from global settings.
    state.max_steps = settings.max_steps;

    uint32_t idle_time;

    /* Handle LVGL tasks. */
    while (state.is_running) {
        // Process one frame of simulation.
        SimulatorLoop::processFrame(dsm, state, 8);

        // Exit immediately if step limit reached - don't wait for more events.
        if (!state.is_running) {
            printf("Simulation completed after %u steps\n", state.step_count);
            break;
        }

        /* Returns the time to the next timer execution. */
        idle_time = lv_timer_handler();
        usleep(idle_time * 1000);
    }
    
    // Process any final UI updates before taking screenshot.
    for (int i = 0; i < 3; ++i) {
        lv_timer_handler();
        usleep(10000); // 10ms.
    }
    
    SimulatorUI::takeExitScreenshot();
}

#endif /*LV_USE_LINUX_FBDEV. */
