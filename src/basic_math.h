#ifndef BASIC_MATH_H
#define BASIC_MATH_H

#include <stdint.h>
#include "lvgl/lvgl.h"

// 线性插值
float linear_interpolation(float w, float x, float y);

// 3x3矩阵乘法
void matrix_multiply_3x3(const float a[3][3], const float b[3][3], float result[3][3]);

// 4x4矩阵乘法
void matrix_multiply_4x4(const float a[4][4], const float b[4][4], float result[4][4]);

// 矩阵向量乘法
void matrix_vector_multiply_3x3(const float matrix[3][3], const float vector[3], float result[3]);

// 4x4矩阵向量乘法
void matrix_vector_multiply_4x4(const float matrix[4][4], const float vector[4], float result[4]);

// 计算边交叉
float calculate_edge_cross(float x, float y,
                          float x0, float y0,
                          float x1, float y1);

// 双线性插值 (基于 lv_obj/lv_canvas)
lv_color32_t bilinear_interpolation(lv_obj_t* image, float src_x, float src_y);

// 双线性插值 (基于 lv_draw_buf_t, 优化为直接返回 RGB565 uint16_t)
uint16_t bilinear_interpolation_draw_buf_rgb565(lv_draw_buf_t* buf, float src_x, float src_y);

#endif // BASIC_MATH_H
