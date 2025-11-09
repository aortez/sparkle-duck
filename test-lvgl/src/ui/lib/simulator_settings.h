/**
 * @file simulator_settings.h
 *
 * global simulator settings
 *
 * The simulator settings is a global variable defined in
 * simulator_settings.c
 *
 * Copyright (c) 2025 EDGEMTech Ltd.
 *
 * Author: EDGEMTech Ltd, Erik Tagirov (erik.tagirov@edgemtech.ch)
 *
 */

#ifndef SIMULATOR_SETTINGS_H
#define SIMULATOR_SETTINGS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stdint.h"

/*********************
 *      INCLUDES
 *********************/

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

typedef struct {
    uint32_t window_width;
    uint32_t window_height;
    bool maximize;
    bool fullscreen;
    uint32_t max_steps; // Maximum number of simulation steps to run (0 = unlimited)
} simulator_settings_t;

/**********************
 * GLOBAL PROTOTYPES
 **********************/

/**********************
 *      MACROS
 **********************/

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*SIMULATOR_SETTINGS_H*/
