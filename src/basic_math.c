#include "basic_math.h"
#define _USE_MATH_DEFINES // For M_PI on Windows
#include <math.h>
#include <string.h>

// 线性插值实现
float linear_interpolation(float w, float x, float y) {
    return w * x + (1.0f - w) * y;
}

// 3x3矩阵乘法 (GEMM风格: C = alpha * A * B + beta * C)
void matrix_multiply_3x3_gemm(const float a[3][3], const float b[3][3], float c[3][3], float alpha, float beta) {
    float temp_result[3][3]; // 存储 A*B 的结果

    // 计算 A * B
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            temp_result[i][j] = 0.0f;
            for (int k = 0; k < 3; k++) {
                temp_result[i][j] += a[i][k] * b[k][j];
            }
        }
    }

    // 计算 C = alpha * (A * B) + beta * C
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            c[i][j] = alpha * temp_result[i][j] + beta * c[i][j];
        }
    }
}

// 3x3矩阵乘法
void matrix_multiply_3x3(const float a[3][3], const float b[3][3], float result[3][3]) {
    // 调用GEMM风格函数，实现 result = 1.0 * (a * b) + 0.0 * result
    matrix_multiply_3x3_gemm(a, b, result, 1.0f, 0.0f);
}

// 矩阵向量乘法 (GEMM风格: result = alpha * matrix * vector + beta * result)
void matrix_vector_multiply_3x3_gemm(const float matrix[3][3], const float vector[3], float result[3], float alpha, float beta)
{
    float temp_result[3]; // 存储 matrix * vector 的结果

    // 计算 matrix * vector
    for (int j = 0; j < 3; j++) {
        temp_result[j] = 0.0f;
        for (int k = 0; k < 3; k++) {
            temp_result[j] += matrix[j][k] * vector[k];
        }
    }

    // 计算 result = alpha * (matrix * vector) + beta * result
    for (int j = 0; j < 3; j++) {
        result[j] = alpha * temp_result[j] + beta * result[j];
    }
}

// 矩阵向量乘法
void matrix_vector_multiply_3x3(const float matrix[3][3], const float vector[3], float result[3])
{
    // 调用GEMM风格函数，实现 result = 1.0 * (matrix * vector) + 0.0 * result
    matrix_vector_multiply_3x3_gemm(matrix, vector, result, 1.0f, 0.0f);
}

// 4x4矩阵乘法 (GEMM风格: C = alpha * A * B + beta * C)
void matrix_multiply_4x4_gemm(const float a[4][4], const float b[4][4], float c[4][4], float alpha, float beta) {
    float temp_result[4][4]; // 存储 A*B 的结果

    // 计算 A * B
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            temp_result[i][j] = 0.0f;
            for (int k = 0; k < 4; k++) {
                temp_result[i][j] += a[i][k] * b[k][j];
            }
        }
    }

    // 计算 C = alpha * (A * B) + beta * C
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            c[i][j] = alpha * temp_result[i][j] + beta * c[i][j];
        }
    }
}

// 4x4矩阵乘法
void matrix_multiply_4x4(const float a[4][4], const float b[4][4], float result[4][4]) {
    // 调用GEMM风格函数，实现 result = 1.0 * (a * b) + 0.0 * result
    matrix_multiply_4x4_gemm(a, b, result, 1.0f, 0.0f);
}

// 4x4矩阵向量乘法 (GEMM风格: result = alpha * matrix * vector + beta * result)
void matrix_vector_multiply_4x4_gemm(const float matrix[4][4], const float vector[4], float result[4], float alpha, float beta)
{
    float temp_result[4]; // 存储 matrix * vector 的结果

    // 计算 matrix * vector
    for (int j = 0; j < 4; j++) {
        temp_result[j] = 0.0f;
        for (int k = 0; k < 4; k++) {
            temp_result[j] += matrix[j][k] * vector[k];
        }
    }

    // 计算 result = alpha * (matrix * vector) + beta * result
    for (int j = 0; j < 4; j++) {
        result[j] = alpha * temp_result[j] + beta * result[j];
    }
}

// 4x4矩阵向量乘法
void matrix_vector_multiply_4x4(const float matrix[4][4], const float vector[4], float result[4])
{
    // 调用GEMM风格函数，实现 result = 1.0 * (matrix * vector) + 0.0 * result
    matrix_vector_multiply_4x4_gemm(matrix, vector, result, 1.0f, 0.0f);
}

// 计算边交叉
float calculate_edge_cross(float x, float y,
                          float x0, float y0,
                          float x1, float y1) {
    return (x1 - x0) * (y - y0) - (y1 - y0) * (x - x0);
}


// 定义宏以控制边界检查，0为关闭，1为开启。默认为关闭以提高性能。
#define BILINEAR_INTERPOLATION_BOUNDS_CHECK 0

/**
 * @brief 使用边缘钳位（edge clamping）执行双线性插值计算像素值。
 *
 * @param obj 指向lv_canvas对象的指针。
 * @param src_x 目标像素在源图像中的x坐标（浮点数）。
 * @param src_y 目标像素在源图像中的y坐标（浮点数）。
 * @return lv_color32_t 插值计算出的颜色值。
 */
lv_color32_t bilinear_interpolation(lv_obj_t* obj, float src_x, float src_y) {
    lv_color32_t interpolated_color;

    int32_t w = lv_obj_get_width(obj);
    int32_t h = lv_obj_get_height(obj);

#if BILINEAR_INTERPOLATION_BOUNDS_CHECK
    // 边界检查 - 提前返回边界外的点。
    if (src_x < 0 || src_y < 0 || src_x >= w || src_y >= h) {
        interpolated_color.red = 0;
        interpolated_color.green = 0;
        interpolated_color.blue = 0;
        interpolated_color.alpha = 0; // 通常Alpha为0表示完全透明
        return interpolated_color;
    }
#endif

    // 计算整数坐标
    int x0 = (int)src_x;
    int y0 = (int)src_y;

    // 计算相邻像素坐标，并进行边界钳位
    int x1 = (x0 + 1 < w) ? x0 + 1 : x0;
    int y1 = (y0 + 1 < h) ? y0 + 1 : y0;

    // 获取四个相邻像素的颜色值
    lv_color32_t p00 = lv_canvas_get_px(obj, x0, y0);
    lv_color32_t p01 = lv_canvas_get_px(obj, x1, y0);
    lv_color32_t p10 = lv_canvas_get_px(obj, x0, y1);
    lv_color32_t p11 = lv_canvas_get_px(obj, x1, y1);

    // 构造转置的像素颜色矩阵，每列是一个颜色通道
    float pixel_matrix_T[4][4] = {
        {(float)p00.red, (float)p01.red, (float)p10.red, (float)p11.red},
        {(float)p00.green, (float)p01.green, (float)p10.green, (float)p11.green},
        {(float)p00.blue, (float)p01.blue, (float)p10.blue, (float)p11.blue},
        {(float)p00.alpha, (float)p01.alpha, (float)p10.alpha, (float)p11.alpha}
    };

    // 计算插值权重
    float wx = src_x - x0;
    float wy = src_y - y0;

    // 构造权重向量
    float weights[4] = {
        (1.0f - wx) * (1.0f - wy), // p00的权重
        wx * (1.0f - wy),          // p01的权重
        (1.0f - wx) * wy,          // p10的权重
        wx * wy                    // p11的权重
    };

    // 使用矩阵向量乘法计算最终颜色
    float final_color_vec[4];
    matrix_vector_multiply_4x4(pixel_matrix_T, weights, final_color_vec);

    // 将结果转换回lv_color32_t
    interpolated_color.red   = (uint8_t)final_color_vec[0];
    interpolated_color.green = (uint8_t)final_color_vec[1];
    interpolated_color.blue  = (uint8_t)final_color_vec[2];
    interpolated_color.alpha = (uint8_t)final_color_vec[3];

    return interpolated_color;
}

// 优化的 RGB565 双线性插值，无中间结构体转换
uint16_t bilinear_interpolation_draw_buf_rgb565(lv_draw_buf_t* buf, float src_x, float src_y) {
    int32_t w = buf->header.w;
    int32_t h = buf->header.h;
    uint32_t stride = buf->header.stride;
    const uint8_t * data = buf->data;

#if BILINEAR_INTERPOLATION_BOUNDS_CHECK
    if (src_x < 0 || src_y < 0 || src_x >= w || src_y >= h) {
        return 0;
    }
#endif

    // 1. 浮点坐标钳位 (Clamp to Edge)
    // 必须在转整数前处理，防止负数或溢出导致权重(wx,wy)超出[0,1]范围，进而产生错误的颜色外推
    if (src_x < 0) src_x = 0;
    if (src_y < 0) src_y = 0;
    if (src_x > w - 1) src_x = w - 1;
    if (src_y > h - 1) src_y = h - 1;

    // 2. 计算整数坐标
    int x0 = (int)src_x;
    int y0 = (int)src_y;

    // 3. 计算相邻像素坐标 (由于src_x已钳位到w-1，x0最大为w-1，此时x1=w-1)
    int x1 = (x0 + 1 < w) ? x0 + 1 : x0;
    int y1 = (y0 + 1 < h) ? y0 + 1 : y0;

    // 4. 计算权重
    float wx = src_x - x0;
    float wy = src_y - y0;

    // 权重因子
    float w00 = (1.0f - wx) * (1.0f - wy);
    float w01 = wx * (1.0f - wy);
    float w10 = (1.0f - wx) * wy;
    float w11 = wx * wy;

    // 4. 读取 4 个邻域像素 (RGB565)
    const uint8_t * row0 = data + y0 * stride;
    const uint8_t * row1 = data + y1 * stride;

    uint16_t p00 = *((const uint16_t*)row0 + x0);
    uint16_t p01 = *((const uint16_t*)row0 + x1);
    uint16_t p10 = *((const uint16_t*)row1 + x0);
    uint16_t p11 = *((const uint16_t*)row1 + x1);

    // 5. 解包并计算 (R:5, G:6, B:5)
    // Red (Mask 0xF800 >> 11)
    float r = w00 * ((p00 & 0xF800) >> 11) +
              w01 * ((p01 & 0xF800) >> 11) +
              w10 * ((p10 & 0xF800) >> 11) +
              w11 * ((p11 & 0xF800) >> 11);

    // Green (Mask 0x07E0 >> 5)
    float g = w00 * ((p00 & 0x07E0) >> 5) +
              w01 * ((p01 & 0x07E0) >> 5) +
              w10 * ((p10 & 0x07E0) >> 5) +
              w11 * ((p11 & 0x07E0) >> 5);

    // Blue (Mask 0x001F)
    float b = w00 * (p00 & 0x001F) +
              w01 * (p01 & 0x001F) +
              w10 * (p10 & 0x001F) +
              w11 * (p11 & 0x001F);

    // 6. 打包回 RGB565
    uint16_t res_r = (uint16_t)r;
    uint16_t res_g = (uint16_t)g;
    uint16_t res_b = (uint16_t)b;

    if (res_r > 31) res_r = 31;
    if (res_g > 63) res_g = 63;
    if (res_b > 31) res_b = 31;

    return (res_r << 11) | (res_g << 5) | res_b;
}
