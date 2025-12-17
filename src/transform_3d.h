#ifndef TRANSFORM_3D_H
#define TRANSFORM_3D_H

#include <stdint.h>
#include <stdbool.h>
#include "lvgl/lvgl.h"

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

// 变换应用函数
int apply_transformations_3d(TransformConfig3D* config);

int calculate_inverse_transform_matrix(
    const Coord2D src_points[4],
    const Coord2D dst_points[4],
    float out_matrix[3][3]
);

void process_faces_and_get_matrices(const TransformConfig3D* config);

bool point_in_quad(const Coord2D point, const Coord2D quad[4]);

#endif // TRANSFORM_3D_H
