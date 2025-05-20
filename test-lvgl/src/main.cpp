#include "World.h"

#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "lvgl/lvgl.h"
#include "src/lib/driver_backends.h"
#include "src/lib/simulator_util.h"
#include "src/lib/simulator_settings.h"

/* Internal functions */
static void configure_simulator(int argc, char **argv);
static void print_lvgl_version(void);
static void print_usage(void);

/* contains the name of the selected backend if user
 * has specified one on the command line */
static char *selected_backend;

/* Global simulator settings, defined in lv_linux_backend.c */
extern simulator_settings_t settings;

/**
 * @brief Print LVGL version
 */
static void print_lvgl_version(void)
{
    fprintf(stdout, "%d.%d.%d-%s\n",
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
    fprintf(stdout, "\nlvglsim [-V] [-B] [-b backend_name] [-W window_width] [-H window_height]\n\n");
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
static void configure_simulator(int argc, char **argv)
{
    int opt = 0;
    char *backend_name;

    selected_backend = NULL;
    driver_backends_register();

    /* Default values */
    settings.window_width = atoi(getenv("LV_SIM_WINDOW_WIDTH") ? : "800");
    settings.window_height = atoi(getenv("LV_SIM_WINDOW_HEIGHT") ? : "480");

    /* Parse the command-line options. */
    while ((opt = getopt (argc, argv, "b:fmW:H:BVh")) != -1) {
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
int main(int argc, char **argv)
{
    configure_simulator(argc, argv);

    /* Initialize LVGL. */
    lv_init();

    /* Initialize the configured backend. */
    if (driver_backends_init_backend(selected_backend) == -1) {
        die("Failed to initialize display backend");
    }

    /* Enable for EVDEV support. */
#if LV_USE_EVDEV
    if (driver_backends_init_backend("EVDEV") == -1) {
        die("Failed to initialize evdev");
    }
#endif

    World world(10, 10);

    world.makeWalls();

    const int CANVAS_WIDTH = 200;
    const int CANVAS_HEIGHT = 200;

    LV_DRAW_BUF_DEFINE_STATIC(draw_buf, CANVAS_WIDTH, CANVAS_HEIGHT, LV_COLOR_FORMAT_ARGB8888);
    LV_DRAW_BUF_INIT_STATIC(draw_buf);

    /*Create a canvas and initialize its palette*/
    lv_obj_t * canvas = lv_canvas_create(lv_screen_active());
    lv_canvas_set_draw_buf(canvas, &draw_buf);

    lv_canvas_fill_bg(canvas, lv_color_hex3(0xccc), LV_OPA_COVER);
    lv_obj_center(canvas);

    lv_layer_t layer;
    lv_canvas_init_layer(canvas, &layer);

    const float cell_width = static_cast<float>(CANVAS_WIDTH) / world.getWidth();
    const float cell_height = static_cast<float>(CANVAS_HEIGHT) / world.getHeight();

    for (int y = 0; y < world.getHeight(); y++) {
        for (int x = 0; x < world.getWidth(); x++) {
            
            lv_draw_rect_dsc_t dsc;
            lv_draw_rect_dsc_init(&dsc);
            dsc.bg_color = lv_palette_main(LV_PALETTE_RED);
            dsc.border_color = lv_palette_main(LV_PALETTE_BLUE);
            dsc.border_width = 1;
            dsc.outline_color = lv_palette_main(LV_PALETTE_GREEN);
            dsc.outline_width = 1;
            dsc.outline_pad = 1;
            dsc.outline_opa = LV_OPA_50;
            dsc.radius = 5;
            dsc.border_width = 3;
            if (world.at(x, y).dirty) {
                dsc.bg_color = lv_palette_main(LV_PALETTE_RED);
            } else {
                dsc.bg_color = lv_palette_main(LV_PALETTE_GREEN);
            }

            const lv_area_t coords = {x * cell_width, y * cell_height, cell_width, cell_height};
            lv_draw_rect(&layer, &dsc, &coords);
        }
    }

    lv_canvas_finish_layer(canvas, &layer);

    /* Enter the run loop of the selected backend. */
    driver_backends_run_loop();

    return 0;
}
