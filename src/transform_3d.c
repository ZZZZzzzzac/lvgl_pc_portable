#include <stdlib.h>
#include "transform_3d.h"
#include "basic_math.h"
#include <math.h>
#include <stdio.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

static TransformConfig3D* transform_config_3d_create(
    uint32_t canvas_width, uint32_t canvas_height)
{
    TransformConfig3D* config = (TransformConfig3D*)malloc(sizeof(TransformConfig3D));
    if (!config) {
        return NULL;
    }

    // 设置几何体数据
    config->vertices_ptr = NULL;
    config->vertices_len = 0;
    config->faces_ptr = NULL;
    config->faces_len = 0;
    config->faces_mat = NULL;


    // 设置画布尺寸
    config->canvas_width = canvas_width;
    config->canvas_height = canvas_height;

    // 设置变换参数
    config->rotation_deg[0] = 0.0f;
    config->rotation_deg[1] = 0.0f;
    config->rotation_deg[2] = 0.0f;

    config->scale[0] = 1.0f;
    config->scale[1] = 1.0f;
    config->scale[2] = 1.0f;

    return config;
}

void transform_config_3d_destroy(TransformConfig3D** config_ptr)
{
    TransformConfig3D* config = *config_ptr;
    if (!config)
        return;

    if (config->faces_ptr)
        free(config->faces_ptr);

    if (config->faces_obj)
        free(config->faces_obj);

    if (config->faces_mat)
        free(config->faces_mat);

    if (config->vertices_ptr)
        free(config->vertices_ptr);

    if (config->vertices_pro)
        free(config->vertices_pro);

    if (config->vertices_tra)
        free(config->vertices_tra);

    free(config);
    *config_ptr = NULL;
}

TransformConfig3D* transform_config_3d_create_cube(
    float size,
    uint32_t canvas_width, uint32_t canvas_height)
{
    TransformConfig3D* config = transform_config_3d_create(
        canvas_width, canvas_height
    );

    if (!config)
        return NULL;

    config->vertices_len = 8;
    config->faces_len = 6;
    config->faces_ptr = (CoordFace*)malloc(sizeof(CoordFace) * config->faces_len);
    config->faces_obj = (lv_obj_t**)malloc(sizeof(lv_obj_t*) * config->faces_len);
    config->faces_mat = (lv_matrix_t*)malloc(sizeof(lv_matrix_t) * config->faces_len);
    config->vertices_ptr = (Coord3D*)malloc(sizeof(Coord3D) * config->vertices_len);
    config->vertices_pro = (Coord2D*)malloc(sizeof(Coord2D) * config->vertices_len);
    config->vertices_tra = (Coord3D*)malloc(sizeof(Coord3D) * config->vertices_len);

    if (
        !config->vertices_pro ||
        !config->vertices_tra ||
        !config->vertices_ptr ||
        !config->faces_ptr ||
        !config->faces_mat
    ) {
        transform_config_3d_destroy(&config);
        return NULL;
    }

    {   // 定义立方体的8个顶点

        float s = size / 2.0f;

        config->vertices_ptr[0] = (Coord3D){-s, -s, -s};  // 左下后
        config->vertices_ptr[1] = (Coord3D){ s, -s, -s};  // 右下后
        config->vertices_ptr[2] = (Coord3D){ s,  s, -s};  // 右上后
        config->vertices_ptr[3] = (Coord3D){-s,  s, -s};  // 左上后
        config->vertices_ptr[4] = (Coord3D){-s, -s,  s};  // 左下前
        config->vertices_ptr[5] = (Coord3D){ s, -s,  s};  // 右下前
        config->vertices_ptr[6] = (Coord3D){ s,  s,  s};  // 右上前
        config->vertices_ptr[7] = (Coord3D){-s,  s,  s};  // 左上前
    }

    {   // 定义立方体的6个面（每个面4个顶点索引）


        // 底面 (z = -s)
        config->faces_ptr[0].vertex_indices[0] = 2;
        config->faces_ptr[0].vertex_indices[1] = 3;
        config->faces_ptr[0].vertex_indices[2] = 0;
        config->faces_ptr[0].vertex_indices[3] = 1;

        // 顶面 (z = s)
        config->faces_ptr[1].vertex_indices[0] = 6;
        config->faces_ptr[1].vertex_indices[1] = 5;
        config->faces_ptr[1].vertex_indices[2] = 4;
        config->faces_ptr[1].vertex_indices[3] = 7;

        // 前面 (y = -s)
        config->faces_ptr[2].vertex_indices[0] = 5;
        config->faces_ptr[2].vertex_indices[1] = 1;
        config->faces_ptr[2].vertex_indices[2] = 0;
        config->faces_ptr[2].vertex_indices[3] = 4;

        // 后面 (y = s)
        config->faces_ptr[3].vertex_indices[0] = 7;
        config->faces_ptr[3].vertex_indices[1] = 3;
        config->faces_ptr[3].vertex_indices[2] = 2;
        config->faces_ptr[3].vertex_indices[3] = 6;

        // 左面 (x = -s)
        config->faces_ptr[4].vertex_indices[0] = 7;
        config->faces_ptr[4].vertex_indices[1] = 4;
        config->faces_ptr[4].vertex_indices[2] = 0;
        config->faces_ptr[4].vertex_indices[3] = 3;

        // 右面 (x = s)
        config->faces_ptr[5].vertex_indices[0] = 6;
        config->faces_ptr[5].vertex_indices[1] = 2;
        config->faces_ptr[5].vertex_indices[2] = 1;
        config->faces_ptr[5].vertex_indices[3] = 5;
    }

    return config;
}

TransformConfig3D* transform_config_3d_create_prism(
    uint32_t n_sides, float height, float radius,
    uint32_t canvas_width, uint32_t canvas_height)
{
    if (n_sides < 3)
        return NULL;

    TransformConfig3D* config = transform_config_3d_create(
        canvas_width, canvas_height
    );

    if (!config)
        return NULL;


    config->vertices_len = n_sides * 2;
    config->vertices_ptr = (Coord3D*)malloc(sizeof(Coord3D) * config->vertices_len);
    config->vertices_pro = (Coord2D*)malloc(sizeof(Coord2D) * config->vertices_len);
    config->vertices_tra = (Coord3D*)malloc(sizeof(Coord3D) * config->vertices_len);
    config->faces_ptr = (CoordFace*)malloc(sizeof(CoordFace) * n_sides);
    config->faces_mat = (lv_matrix_t*)malloc(sizeof(lv_matrix_t) * n_sides);

    if (
        !config->vertices_pro ||
        !config->vertices_tra ||
        !config->vertices_ptr ||
        !config->faces_ptr ||
        !config->faces_mat
    ) {
        transform_config_3d_destroy(&config);
        return NULL;
    }

    {   // 计算底部和顶部顶点的坐标
        float s = height / 2.0f;

        for (int i = 0; i < n_sides; i++) {
            float angle = 2.0f * M_PI * i / n_sides;
            float x = radius * cosf(angle);
            float z = radius * sinf(angle);

            // 顶部顶点 (Y坐标为正)
            config->vertices_ptr[i * 2] = (Coord3D){x, s, z};
            // 底部顶点 (Y坐标为负)
            config->vertices_ptr[i * 2 + 1] = (Coord3D){x, -s, z};
        }
    }

    {   // 定义棱柱的侧面
        config->faces_len = n_sides;
        for (int i = 0; i < n_sides; i++) {
            // 侧面的顶点索引
            int32_t p0 = i * 2;                    // bottom-current
            int32_t p1 = i * 2 + 1;                // top-current
            int32_t p2 = ((i + 1) % n_sides) * 2 + 1; // top-next
            int32_t p3 = ((i + 1) % n_sides) * 2;     // bottom-next

            // 使用逆时针（CCW）环绕顺序 (bottom-left -> top-left -> top-right -> bottom-right)
            config->faces_ptr[i].vertex_indices[0] = p3;
            config->faces_ptr[i].vertex_indices[1] = p0;
            config->faces_ptr[i].vertex_indices[2] = p1;
            config->faces_ptr[i].vertex_indices[3] = p2;
        }
    }

    return config;
}

// 创建旋转矩阵函数
void create_rotation_matrix_x_3x3(float angle_deg, float matrix[3][3])
{
    float rad = angle_deg * M_PI / 180.0f;
    float c = cosf(rad);
    float s = sinf(rad);

    matrix[0][0] = 1.0f; matrix[0][1] = 0.0f;  matrix[0][2] = 0.0f;
    matrix[1][0] = 0.0f; matrix[1][1] = c;     matrix[1][2] = -s;
    matrix[2][0] = 0.0f; matrix[2][1] = s;     matrix[2][2] = c;
}

void create_rotation_matrix_y_3x3(float angle_deg, float matrix[3][3])
{
    float rad = angle_deg * M_PI / 180.0f;
    float c = cosf(rad);
    float s = sinf(rad);

    matrix[0][0] = c;     matrix[0][1] = 0.0f;  matrix[0][2] = s;
    matrix[1][0] = 0.0f;  matrix[1][1] = 1.0f;  matrix[1][2] = 0.0f;
    matrix[2][0] = -s;    matrix[2][1] = 0.0f;  matrix[2][2] = c;
}

void create_rotation_matrix_z_3x3(float angle_deg, float matrix[3][3])
{
    float rad = angle_deg * M_PI / 180.0f;
    float c = cosf(rad);
    float s = sinf(rad);

    matrix[0][0] = c;     matrix[0][1] = -s;    matrix[0][2] = 0.0f;
    matrix[1][0] = s;     matrix[1][1] = c;     matrix[1][2] = 0.0f;
    matrix[2][0] = 0.0f;  matrix[2][1] = 0.0f;  matrix[2][2] = 1.0f;
}

void create_scaling_matrix_3x3(float x, float y, float z, float matrix[3][3])
{
    matrix[0][0] = x;     matrix[0][1] = 0.0f;  matrix[0][2] = 0.0f;
    matrix[1][0] = 0.0f;  matrix[1][1] = y;     matrix[1][2] = 0.0f;
    matrix[2][0] = 0.0f;  matrix[2][1] = 0.0f;  matrix[2][2] = z;
}

static int is_equal(float a, float b, float tolerance) {
    return fabsf(a - b) < tolerance;
}

int apply_transformations_3d(TransformConfig3D* config)
{
    if (!config) {
        return -1; // 参数错误
    }

    if (!config->vertices_ptr || config->vertices_len == 0) {
        return -2; // 顶点数据错误
    }

    float matrix_a[3][3] = {
        {1.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f},
        {0.0f, 0.0f, 1.0f}
    };
    float matrix_b[3][3] = {0};
    float matrix_c[3][3] = {0};
    float (*matrix_in)[3]  = matrix_a;
    float (*matrix_tmp)[3] = matrix_b;
    float (*matrix_out)[3] = matrix_c;

    // 应用缩放
    if (!(is_equal(config->scale[0], 1.0f, 1e-6f) &&
          is_equal(config->scale[1], 1.0f, 1e-6f) &&
          is_equal(config->scale[2], 1.0f, 1e-6f)))
    {
        create_scaling_matrix_3x3(config->scale[0], config->scale[1], config->scale[2], matrix_b);
        matrix_multiply_3x3(matrix_in, matrix_b, matrix_out);
        matrix_tmp = matrix_in;
        matrix_in = matrix_out;
        matrix_out = matrix_tmp;
    }

    // 应用旋转
    if (!is_equal(config->rotation_deg[0], 0, 1e-6f))
    {
        create_rotation_matrix_x_3x3(config->rotation_deg[0], matrix_b);
        matrix_multiply_3x3(matrix_in, matrix_b, matrix_out);
        matrix_tmp = matrix_in;
        matrix_in = matrix_out;
        matrix_out = matrix_tmp;
    }

    if (!is_equal(config->rotation_deg[1], 0, 1e-6f))
    {
        create_rotation_matrix_y_3x3(config->rotation_deg[1], matrix_b);
        matrix_multiply_3x3(matrix_in, matrix_b, matrix_out);
        matrix_tmp = matrix_in;
        matrix_in = matrix_out;
        matrix_out = matrix_tmp;
    }

    if (!is_equal(config->rotation_deg[2], 0, 1e-6f))
    {
        create_rotation_matrix_z_3x3(config->rotation_deg[2], matrix_b);
        matrix_multiply_3x3(matrix_in, matrix_b, matrix_out);
        matrix_tmp = matrix_in;
        matrix_in = matrix_out;
        matrix_out = matrix_tmp;
    }

    // 应用变换到每个顶点
    for (uint32_t i = 0; i < config->vertices_len; i++) {
        const Coord3D* vertex = &config->vertices_ptr[i];
        float vertex_array[3] = {vertex->x, vertex->y, vertex->z};
        float transformed_array[3];

        // 应用旋转变换
        matrix_vector_multiply_3x3(matrix_a, vertex_array, transformed_array);

        // 存储变换后的顶点
        config->vertices_tra[i] = (Coord3D){
            transformed_array[0],
            transformed_array[1],
            transformed_array[2]
        };

        // 简单正交投影 (忽略透视)
        float screen_x = (transformed_array[0] + 1.0f) * 0.5f * config->canvas_width;
        float screen_y = (1.0f - transformed_array[1]) * 0.5f * config->canvas_height;

        // 存储投影后的顶点
        config->vertices_pro[i] = (Coord2D){screen_x, screen_y};
    }

    return 0; // 成功
}

int calculate_inverse_transform_matrix(
    const Coord2D src_points[4],
    const Coord2D dst_points[4],
    float out_matrix[3][3])
{
    if (!src_points || !dst_points || !out_matrix) {
        return -1; // Invalid arguments
    }

    // Augmented matrix for the 6x6 linear system
    float augmented[6][7] = {0};

    // Populate the augmented matrix
    for (int i = 0; i < 4; ++i) {
        float X = src_points[i].x;
        float Y = src_points[i].y;
        float x = dst_points[i].x;
        float y = dst_points[i].y;

        if (2 * i < 6) {
            int row = 2 * i;
            augmented[row][0] = x;
            augmented[row][1] = y;
            augmented[row][2] = 1.0f;
            augmented[row][6] = X;
        }
        if (2 * i + 1 < 6) {
            int row = 2 * i + 1;
            augmented[row][3] = x;
            augmented[row][4] = y;
            augmented[row][5] = 1.0f;
            augmented[row][6] = Y;
        }
    }

    // Gaussian elimination
    for (int col = 0; col < 6; ++col) {
        // Partial pivoting
        int max_row = col;
        float max_val = fabsf(augmented[col][col]);
        for (int row = col + 1; row < 6; ++row) {
            if (fabsf(augmented[row][col]) > max_val) {
                max_row = row;
                max_val = fabsf(augmented[row][col]);
            }
        }

        // Swap rows
        if (max_row != col) {
            for (int c = 0; c < 7; ++c) {
                float tmp = augmented[col][c];
                augmented[col][c] = augmented[max_row][c];
                augmented[max_row][c] = tmp;
            }
        }

        // Check for singular matrix
        if (fabsf(augmented[col][col]) < 1e-10f) {
            // Matrix is singular, cannot solve
            return -1;
        }

        // Eliminate current column
        float pivot = 1.0f / augmented[col][col];
        for (int row = col + 1; row < 6; ++row) {
            float factor = augmented[row][col] * pivot;
            augmented[row][col] = 0.0f; // Explicitly set to zero
            for (int c = col + 1; c < 7; ++c) {
                augmented[row][c] -= factor * augmented[col][c];
            }
        }
    }

    // Back substitution
    float h[6];
    for (int row = 5; row >= 0; --row) {
        h[row] = augmented[row][6];
        for (int col = row + 1; col < 6; ++col) {
            h[row] -= augmented[row][col] * h[col];
        }
        h[row] /= augmented[row][row];
    }

    // Populate the output matrix
    out_matrix[0][0] = h[0];
    out_matrix[0][1] = h[1];
    out_matrix[0][2] = h[2];
    out_matrix[1][0] = h[3];
    out_matrix[1][1] = h[4];
    out_matrix[1][2] = h[5];
    out_matrix[2][0] = 0.0f;
    out_matrix[2][1] = 0.0f;
    out_matrix[2][2] = 1.0f;

    return 0; // Success
}

/**
 * @brief Checks if a face is visible using back-face culling.
 *
 * @param face_vertices An array of 4 vertex indices for the face.
 * @param transformed_vertices An array of all transformed 3D vertices.
 * @return true if the face is visible, false otherwise.
 */
static bool is_face_visible(const int32_t face_vertices[4], const Coord3D* transformed_vertices)
{
    // Get the transformed coordinates of the first three vertices of the face
    const Coord3D* v0 = &transformed_vertices[face_vertices[0]];
    const Coord3D* v1 = &transformed_vertices[face_vertices[1]];
    const Coord3D* v2 = &transformed_vertices[face_vertices[2]];

    // Calculate two edge vectors in screen space (ignoring Z)
    float edge1_x = v1->x - v0->x;
    float edge1_y = v1->y - v0->y;
    float edge2_x = v2->x - v0->x;
    float edge2_y = v2->y - v0->y;

    // Calculate the Z component of the cross product
    // This determines the winding order on the screen.
    float normal_z = edge1_x * edge2_y - edge1_y * edge2_x;

    // A positive Z component means the face is front-facing (counter-clockwise winding).
    return normal_z > 0;
}

void process_faces_and_get_matrices(const TransformConfig3D* config)
{
    if (!config) {
        return;
    }

    int visible_face_count = 0;

    // Iterate over each face
    for (uint32_t i = 0; i < config->faces_len; ++i)
    {
        const CoordFace* face = &config->faces_ptr[i];
        lv_obj_t* obj = config->faces_obj[i];

        // 1. Back-face culling
        // 这里我只画了正面可见的几个面，不过我看矩形那个demo，棱柱是画了所有面的。
        if (!is_face_visible(face->vertex_indices, config->vertices_tra)) {
            lv_matrix_identity(&config->faces_mat[i]);
            lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
            continue; // Skip back-facing polygons
        }
        lv_obj_remove_flag(obj, LV_OBJ_FLAG_HIDDEN);

        // config->vertices_pro就是3d图形的四边形面投影在表盘上的坐标
        // 整个3x3矩阵的作用就是，将图片的4个角的矩形坐标转换到3d图形面的四个角的平行四边形坐标
        const Coord2D dst_points[4] = {
            config->vertices_pro[face->vertex_indices[1]],
            config->vertices_pro[face->vertex_indices[2]],
            config->vertices_pro[face->vertex_indices[3]],
            config->vertices_pro[face->vertex_indices[0]],
        };
        int32_t x = lv_obj_get_x(obj);
        int32_t y = lv_obj_get_y(obj);
        int32_t w = lv_obj_get_width(obj);
        int32_t h = lv_obj_get_height(obj);
        const Coord2D src_points[4] = {
            {x,y},{x,y+h},{x+w,y+h},{x+w,y}
        };
        // 3. Calculate the inverse perspective transform matrix
        // 不确定lvgl里需要的矩阵是正向的还是逆矩阵。总之调换src_point和dst_point就能得到正矩阵或逆矩阵，实际测测看吧。
       int result = calculate_inverse_transform_matrix(dst_points, src_points, config->faces_mat[i].m);

       lv_obj_set_transform(obj, &config->faces_mat[i]);
    }
}

/**
 * @brief 检查点是否在四边形内部（使用向量叉积算法）。
 *
 * 该算法通过计算点与四边形每条边的向量叉积来判断点是否在四边形内部。
 * 对于凸四边形，如果点在所有边的同一侧（即所有叉积结果的符号相同），则点在四边形内部。
 *
 * @param point 待检查的点 {x, y}。
 * @param quad 四边形的四个顶点数组，顶点应按顺时针或逆时针顺序排列。
 * @return 如果点在四边形内部或在边上，则返回true；否则返回false。
 */
bool point_in_quad(const Coord2D point, const Coord2D quad[4])
{
    float x = point.x;
    float y = point.y;

    // 获取四边形的四个顶点
    float x0 = quad[0].x, y0 = quad[0].y;
    float x1 = quad[1].x, y1 = quad[1].y;
    float x2 = quad[2].x, y2 = quad[2].y;
    float x3 = quad[3].x, y3 = quad[3].y;

    // 计算点相对于第一条边的叉积，作为参考符号
    float cross0 = calculate_edge_cross(x, y, x0, y0, x1, y1);

    // 计算点相对于第二条边的叉积
    float cross1 = calculate_edge_cross(x, y, x1, y1, x2, y2);
    // 检查符号是否与参考符号相反。如果是，则点在外部。
    if (cross0 * cross1 < 0) {
        return false;
    }

    // 计算点相对于第三条边的叉积
    float cross2 = calculate_edge_cross(x, y, x2, y2, x3, y3);
    // 再次检查符号
    if (cross0 * cross2 < 0) {
        return false;
    }

    // 计算点相对于第四条边的叉积
    float cross3 = calculate_edge_cross(x, y, x3, y3, x0, y0);
    // 最后一次检查符号
    if (cross0 * cross3 < 0) {
        return false;
    }

    // 如果所有叉积的符号都一致（或为零），则点在四边形内部或边上
    return true;
}
