
#include "World.h"
#include "src/lib/driver_backends.h"
#include "src/lib/simulator_settings.h"
#include "src/lib/simulator_util.h"

#include <string.h>
#include <unistd.h>

#include "lvgl/lvgl.h"

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
static void print_lvgl_version(void) {
  fprintf(stdout, "%d.%d.%d-%s\n", LVGL_VERSION_MAJOR, LVGL_VERSION_MINOR,
          LVGL_VERSION_PATCH, LVGL_VERSION_INFO);
}

/**
 * @brief Print usage information
 */
static void print_usage(void) {
  fprintf(stdout, "\nlvglsim [-V] [-B] [-b backend_name] [-W window_width] [-H "
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
static void configure_simulator(int argc, char **argv) {
  int opt = 0;
  char *backend_name;

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
int main(int argc, char **argv) {
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

  lv_obj_t *draw_area = lv_obj_create(lv_scr_act());
  lv_obj_set_size(draw_area, draw_area_width, draw_area_height);

  // Init the world.
  World world(20, 20, draw_area);
  world.makeWalls();
  world.fillWithDirt();

  // Enter the run loop, using the selected backend.
  driver_backends_run_loop(world);

  return 0;
}
