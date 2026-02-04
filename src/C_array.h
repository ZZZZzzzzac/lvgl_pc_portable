#ifndef LVGL_C_ARRAY_H
#define LVGL_C_ARRAY_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"

/* 图像索引枚举 */
typedef enum {
    BP = 0,
    BPM_F = 1,
    DIAL2_BG = 2,
    HRS = 3,
    HRS_8 = 4,
    HRS_PNG = 5,
    MESSAGE_LINE_ICON = 6,
    MESSAGE_NOTE = 7,
    MMHG_F = 8,
    POINT_53 = 9,
    RING = 10,
    RING_16 = 11,
    RING_PNG = 12,
    SPO2 = 13,
    SPO2_F = 14,
    WF2_H = 15,
    WF2_M = 16,
    WF2_S = 17,
    C_ARRAY_IMG_NUM = 18
} C_array_img_index_t;

/* 图像描述符数组（定义在 C_array.c 中） */
extern const lv_image_dsc_t C_array_images[C_ARRAY_IMG_NUM];

/* 获取图像描述符 */
const lv_image_dsc_t * C_array_get_img(C_array_img_index_t idx);

/* 快捷宏 */
#define PIC_ADDR(a) C_array_get_img(a)

#ifdef __cplusplus
}
#endif

#endif /* LVGL_C_ARRAY_H */
