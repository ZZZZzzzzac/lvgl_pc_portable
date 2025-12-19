#ifndef TRANSFORM_3D_H
#define TRANSFORM_3D_H

#include <stdint.h>
#include <stdbool.h>
#include "lvgl/lvgl.h"

#define DISP_HOR_RES 240
#define DISP_VER_RES 296
#define DISP_CUBE_SIZE 240

// 顶点结构体 (x, y, z)
typedef struct {
    float x, y, z;
} Coord3D;

// 投影顶点结构体
typedef struct {
    float x, y;
} Coord2D;

// 面结构体 (包含4个顶点索引)
typedef struct {
    int32_t vertex_indices[4];
} CoordFace;

// 3D变换配置结构体
typedef struct {
    // 几何体数据
    Coord3D* vertices_ptr;        // 顶点数组指针
    uint32_t vertices_len;         // 顶点数量

    uint32_t    faces_len;            // 面数量
    CoordFace*  faces_ptr;    // 面数组指针
    lv_obj_t**  faces_obj;
    lv_matrix_t*faces_mat;


    // 画布尺寸
    uint32_t canvas_width;
    uint32_t canvas_height;

    // 模型变换参数
    float rotation_deg[3];         // 旋转角度 (rx, ry, rz)
    float scale[3];                // 缩放因子 (sx, sy, sz)

    // 变换结果的预分配内存
    Coord2D* vertices_pro; // 投影顶点数组
    Coord3D* vertices_tra;      // 变换后的顶点数组
} TransformConfig3D;

void transform_config_3d_destroy(TransformConfig3D** config);

// High-level constructors for specific shapes
TransformConfig3D* transform_config_3d_create_cube(
    float size,
    uint32_t canvas_width, uint32_t canvas_height
);

TransformConfig3D* transform_config_3d_create_prism(
    uint32_t n_sides, float height, float radius,
    uint32_t canvas_width, uint32_t canvas_height
);

void demo(void);

#endif // TRANSFORM_3D_H
