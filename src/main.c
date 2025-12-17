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

#include "transform_3d.h"
#include "demo_direct_draw.h"
#include "demo_snapshot_verification.h"

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
#define DISP_HOR_RES 240
#define DISP_VER_RES 296
#define DISP_CUBE_SIZE 120

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

static TransformConfig3D* g_cube = NULL;
static int16_t last_x = 0;
static int16_t last_y = 0;
static bool is_dragging = false;

// 修改后的鼠标事件处理函数 - 支持动态拖动
static void mouse_handler(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_point_t pos;
    lv_indev_t * indev = lv_indev_get_act();

    switch(code) {
        case LV_EVENT_PRESSED:
            if(indev && lv_indev_get_type(indev) == LV_INDEV_TYPE_POINTER) {
                lv_indev_get_point(indev, &pos);
                last_x = pos.x;
                last_y = pos.y;
                is_dragging = true;
            }
            break;

        case LV_EVENT_PRESSING:
            if(is_dragging && indev && lv_indev_get_type(indev) == LV_INDEV_TYPE_POINTER) {
                lv_indev_get_point(indev, &pos);

                // 计算增量移动距离
                int16_t dx = pos.x - last_x;
                int16_t dy = pos.y - last_y;

                // 添加移动阈值，过滤微小移动
                const int16_t MOVE_THRESHOLD = 2;

                if(abs(dx) > MOVE_THRESHOLD || abs(dy) > MOVE_THRESHOLD) {
                    // 更新立方体旋转角度
                    g_cube->rotation_deg[1] -= dx * 0.3f;
                    g_cube->rotation_deg[0] -= dy * 0.3f;

                    // 应用变换
                    // LV_PROFILER_BEGIN_TAG("apply_transformations_3d");
                    apply_transformations_3d(g_cube);
                    // LV_PROFILER_END_TAG("apply_transformations_3d");

                    // LV_PROFILER_BEGIN_TAG("process_faces_and_get_matrices");
                    process_faces_and_get_matrices(g_cube);
                    // LV_PROFILER_END_TAG("process_faces_and_get_matrices");

                    // 更新位置
                    last_x = pos.x;
                    last_y = pos.y;
                }
            }
            break;

        case LV_EVENT_RELEASED:
            is_dragging = false;
            break;

        default:
            break;
    }
}

void demo(void)
{
    g_cube = transform_config_3d_create_cube(1.0f, DISP_CUBE_SIZE, DISP_CUBE_SIZE);

    // 总画布
    lv_obj_t * cont = lv_obj_create(lv_screen_active());
    lv_obj_set_size(cont, DISP_HOR_RES, DISP_VER_RES);
    lv_obj_center(cont);
    lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t * obj1 = lv_obj_create(cont);
    lv_obj_t * obj2 = lv_obj_create(cont);
    lv_obj_t * obj3 = lv_obj_create(cont);
    lv_obj_t * obj4 = lv_obj_create(cont);
    lv_obj_t * obj5 = lv_obj_create(cont);
    lv_obj_t * obj6 = lv_obj_create(cont);

    lv_obj_set_style_bg_color(obj1, lv_color_make(255, 255,   0), 0);
    lv_obj_set_style_bg_color(obj2, lv_color_make(  0,   0,   0), 0);
    lv_obj_set_style_bg_color(obj3, lv_color_make(255,   0,   0), 0);
    lv_obj_set_style_bg_color(obj4, lv_color_make(  0, 255,   0), 0);
    lv_obj_set_style_bg_color(obj5, lv_color_make(  0,   0, 255), 0);
    lv_obj_set_style_bg_color(obj6, lv_color_make(255, 255, 255), 0);

    lv_obj_t * objs[] = {obj1, obj2, obj3, obj4, obj5, obj6};
    for (int i = 0; i < 6; i++)
    {
        lv_obj_set_size(objs[i], 200, 200);
        g_cube->faces_obj[i] = objs[i];

        lv_obj_t * label = lv_label_create(objs[i]);
        lv_obj_set_style_text_font(label, &lv_font_montserrat_20, 0);
        lv_label_set_text(label, "text");
        lv_obj_center(label);

        lv_obj_t * sub_obj1 = lv_obj_create(objs[i]);
        lv_obj_set_size(sub_obj1, 40, 40);
        lv_obj_set_style_bg_color(sub_obj1, lv_color_make(255, 0, 255), 0);
        lv_obj_align(sub_obj1, LV_ALIGN_TOP_LEFT, 5, 5);

        lv_obj_t * sub_obj2 = lv_obj_create(objs[i]);
        lv_obj_set_size(sub_obj2, 20, 40);
        lv_obj_set_style_bg_color(sub_obj2, lv_color_make(0, 255, 255), 0);
        lv_obj_align(sub_obj2, LV_ALIGN_BOTTOM_RIGHT, -5, -5);

        lv_obj_update_layout(objs[i]);
    }

    // 创建透明的鼠标事件捕获层
    lv_obj_t * mouse_layer = lv_obj_create(cont);
    lv_obj_set_size(mouse_layer, DISP_HOR_RES, DISP_VER_RES);
    lv_obj_set_pos(mouse_layer, 0, 0);
    lv_obj_clear_flag(mouse_layer, LV_OBJ_FLAG_SCROLLABLE);

    // 设置透明样式
    lv_obj_set_style_bg_opa(mouse_layer, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_opa(mouse_layer, LV_OPA_TRANSP, 0);
    lv_obj_set_style_outline_opa(mouse_layer, LV_OPA_TRANSP, 0);
    lv_obj_set_style_shadow_opa(mouse_layer, LV_OPA_TRANSP, 0);

    // 使能点击并添加事件回调
    lv_obj_add_flag(mouse_layer, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(mouse_layer, LV_OBJ_FLAG_SCROLL_ON_FOCUS);
    lv_obj_add_event_cb(mouse_layer, mouse_handler, LV_EVENT_ALL, NULL);

    g_cube->rotation_deg[0] = -10.0f;
    g_cube->rotation_deg[1] = 40.0f;
    g_cube->rotation_deg[2] = 0.0f;

    apply_transformations_3d(g_cube);
    process_faces_and_get_matrices(g_cube);
}

void very_simple_demo(void)
{
    // lv_obj_t * obj1 = lv_image_create(lv_screen_active());
    // lv_image_set_src(obj1, &lena_240);

    lv_obj_t * obj1 = lv_obj_create(lv_screen_active());
    lv_obj_set_size(obj1, DISP_CUBE_SIZE, DISP_CUBE_SIZE);

    lv_obj_center(obj1);

    // lv_matrix_t matrix;
    // lv_matrix_identity(&matrix);
    // lv_matrix_rotate(&matrix, 40);
    // matrix.m[0][2] = 50; // x 平移
    // matrix.m[1][2] = 20;  // y 平移
    // lv_obj_set_transform(obj1, &matrix);
}

FILE *mylog;
FILE *log_file;
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
    fprintf(mylog, "%s", buf);
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
    if(log_file) {
        fprintf(log_file, "%s", buf);
        fflush(log_file);
    }
}

#if LV_USE_OS != LV_OS_FREERTOS

int main(int argc, char **argv)
{
    (void)argc; /*Unused*/
    (void)argv; /*Unused*/
    mylog = fopen("lvgl_profiler_log.txt", "w");
    log_file = fopen("lvgl_log.txt", "w");
    /*Initialize LVGL*/
    lv_init();
#if LV_USE_LOG != 0
    lv_log_register_print_cb(log_to_file_cb);
#endif
    /*Initialize the HAL (display, input devices, tick) for LVGL*/
    sdl_hal_init(DISP_HOR_RES, DISP_VER_RES);

    /* Run the default demo */
    /* To try a different demo or example, replace this with one of: */
    /* - lv_demo_benchmark(); */
    /* - lv_demo_stress(); */
    /* - lv_example_label_1(); */
    /* - etc. */
    // lv_demo_widgets();
    // lv_demo_benchmark();
    // my_profiler_init();
    // demo();
    // very_simple_demo();
    // lv_example_canvas_1();
    // lv_example_obj_3();
    // lv_demo_direct_draw();
    run_demo_snapshot_verification();

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
    fclose(mylog);
    return 0;
}


#endif

/**********************
 *   STATIC FUNCTIONS
 **********************/
