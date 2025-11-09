// TODO: Redesign for client/server architecture
#if 0 // Temporarily disabled
/**
 * @file sdl.c
 *
 * The backend for the SDL simulator
 *
 * Based on the original file from the repository
 *
 * - Move to a separate file
 *   2025 EDGEMTech Ltd.
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
#include "server/StateMachine.h"
#include "simulator_loop.h"
#if LV_USE_SDL
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
static void run_loop_sdl(DirtSim::DirtSimStateMachine& dsm);
static lv_display_t* init_sdl(void);

/**********************
 *  STATIC VARIABLES
 **********************/

static char* backend_name = "SDL";

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

/**
 * Register the backend
 * @param backend the backend descriptor
 * @description configures the descriptor
 */
int backend_init_sdl(backend_t* backend)
{
    LV_ASSERT_NULL(backend);

    backend->handle->display = malloc(sizeof(display_backend_t));
    LV_ASSERT_NULL(backend->handle->display);

    backend->handle->display->init_display = init_sdl;
    backend->handle->display->run_loop = run_loop_sdl;
    backend->name = backend_name;
    backend->type = BACKEND_DISPLAY;

    return 0;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

/**
 * Initialize the SDL display driver
 *
 * @return the LVGL display
 */
static lv_display_t* init_sdl(void)
{
    lv_display_t* disp;

    disp = lv_sdl_window_create(settings.window_width, settings.window_height);

    if (disp == NULL) {
        return NULL;
    }

    return disp;
}

/**
 * The run loop of the SDL driver
 */
static void run_loop_sdl(DirtSim::DirtSimStateMachine& dsm)
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
        
        bool frame_limiting_enabled = true; // Default to enabled.
        // TODO: Get frame limiting from settings or config.
        if (frame_limiting_enabled) {
            usleep(idle_time * 1000);
        }
    }
    
    // Process any final UI updates before taking screenshot.
    for (int i = 0; i < 3; ++i) {
        lv_timer_handler();
        usleep(10000); // 10ms.
    }
    
    SimulatorUI::takeExitScreenshot();
}
#endif /*#if LV_USE_SDL. */
#endif
