/**
 * @file main.c
 *
 */

/*********************
 *      INCLUDES
 *********************/

#ifndef _DEFAULT_SOURCE
  #define _DEFAULT_SOURCE /* needed for usleep() */
#endif

#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#if defined(_WIN32) || defined(_WIN64)
  #include <windows.h>
#endif

#ifdef _MSC_VER
#else
  #include <unistd.h>
  #include <pthread.h>
#endif
#include "lvgl/lvgl.h"
#include "lvgl/examples/lv_examples.h"
#include "lvgl/demos/lv_demos.h"
#include "lvgl/src/misc/lv_profiler_builtin_private.h"
#include <SDL.h>

#include "hal/hal.h"

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void log_to_file_cb(lv_log_level_t level, const char * buf);
/**********************
 *  STATIC VARIABLES
 **********************/

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/



void very_simple_demo(void)
{
    // lv_obj_t * obj1 = lv_image_create(lv_screen_active());
    // lv_image_set_src(obj1, &lena_240);

    lv_obj_t * obj1 = lv_obj_create(lv_screen_active());
    lv_obj_set_size(obj1, 200, 200);

    lv_obj_center(obj1);

    // lv_matrix_t matrix;
    // lv_matrix_identity(&matrix);
    // lv_matrix_rotate(&matrix, 40);
    // matrix.m[0][2] = 50; // x 平移
    // matrix.m[1][2] = 20;  // y 平移
    // lv_obj_set_transform(obj1, &matrix);
}

FILE *prof_log;
FILE *event_log;
#if LV_USE_PROFILER
static uint64_t my_get_tick_cb(void)
{
#if defined(_WIN32) || defined(_WIN64)
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    return counter.QuadPart;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
#endif
}
static void my_log_print_cb(const char * buf)
{
    fprintf(prof_log, "%s", buf);
}
void my_profiler_init(void)
{
    lv_profiler_builtin_config_t config;
    lv_profiler_builtin_config_init(&config);
#if defined(_WIN32) || defined(_WIN64)
    LARGE_INTEGER freq;
    QueryPerformanceFrequency(&freq);
    config.tick_per_sec = freq.QuadPart; /* Windows下动态获取高精度计时器频率 */
#else
    config.tick_per_sec = 1000000000; /* Linux/POSIX下为纳秒精度 */
#endif
    config.tick_get_cb = my_get_tick_cb;
    config.flush_cb = my_log_print_cb;
    lv_profiler_builtin_init(&config);
}
#else
void my_profiler_init(void)
{
    /*No profiler*/
}
#endif

static void log_to_file_cb(lv_log_level_t level, const char * buf)
{
    LV_UNUSED(level);
    if(event_log) {
        fprintf(event_log, "%s", buf);
        fflush(event_log);
    }
}

#if LV_USE_OS != LV_OS_FREERTOS

int main(int argc, char **argv)
{
  (void)argc; /*Unused*/
  (void)argv; /*Unused*/
  prof_log = fopen("lvgl_profiler.log", "w");
  event_log = fopen("lvgl.log", "w");
  /*Initialize LVGL*/
  lv_init();

  /*Initialize the HAL (display, input devices, tick) for LVGL*/
  sdl_hal_init(298, 320);

  /* Run the default demo */
  /* To try a different demo or example, replace this with one of: */
  // lv_demo_benchmark();
  // lv_demo_stress();
  // lv_example_label_1();
  // lv_example_get_started_3();
  // lv_example_get_started_4();
  // lv_example_anim_3();
  lv_example_get_localpic();
  /* - etc. */
  // lv_demo_widgets();

  while(1) {
    /* Periodically call the lv_task handler.
     * It could be done in a timer interrupt or an OS task too.*/
    uint32_t sleep_time_ms = lv_timer_handler();
    if(sleep_time_ms == LV_NO_TIMER_READY){
	sleep_time_ms =  LV_DEF_REFR_PERIOD;
    }
#ifdef _MSC_VER
    Sleep(sleep_time_ms);
#else
    usleep(sleep_time_ms * 1000);
#endif
  }
  fclose(prof_log);
  fclose(event_log);
  return 0;
}


#endif

/**********************
 *   STATIC FUNCTIONS
 **********************/
