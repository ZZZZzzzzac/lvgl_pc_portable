#include <math.h>
#include "transform_3d.h"
#include "basic_math.h"
// 创建旋转矩阵函数
static void create_rotation_matrix_x_3x3(float angle_deg, float matrix[3][3])
{
    float rad = angle_deg * M_PI / 180.0f;
    float c = cosf(rad);
    float s = sinf(rad);

    matrix[0][0] = 1.0f; matrix[0][1] = 0.0f;  matrix[0][2] = 0.0f;
    matrix[1][0] = 0.0f; matrix[1][1] = c;     matrix[1][2] = -s;
    matrix[2][0] = 0.0f; matrix[2][1] = s;     matrix[2][2] = c;
}

static void create_rotation_matrix_y_3x3(float angle_deg, float matrix[3][3])
{
    float rad = angle_deg * M_PI / 180.0f;
    float c = cosf(rad);
    float s = sinf(rad);

    matrix[0][0] = c;     matrix[0][1] = 0.0f;  matrix[0][2] = s;
    matrix[1][0] = 0.0f;  matrix[1][1] = 1.0f;  matrix[1][2] = 0.0f;
    matrix[2][0] = -s;    matrix[2][1] = 0.0f;  matrix[2][2] = c;
}

static void create_rotation_matrix_z_3x3(float angle_deg, float matrix[3][3])
{
    float rad = angle_deg * M_PI / 180.0f;
    float c = cosf(rad);
    float s = sinf(rad);

    matrix[0][0] = c;     matrix[0][1] = -s;    matrix[0][2] = 0.0f;
    matrix[1][0] = s;     matrix[1][1] = c;     matrix[1][2] = 0.0f;
    matrix[2][0] = 0.0f;  matrix[2][1] = 0.0f;  matrix[2][2] = 1.0f;
}

static void create_scaling_matrix_3x3(float x, float y, float z, float matrix[3][3])
{
    matrix[0][0] = x;     matrix[0][1] = 0.0f;  matrix[0][2] = 0.0f;
    matrix[1][0] = 0.0f;  matrix[1][1] = y;     matrix[1][2] = 0.0f;
    matrix[2][0] = 0.0f;  matrix[2][1] = 0.0f;  matrix[2][2] = z;
}

static int is_equal(float a, float b, float tolerance) {
    return fabsf(a - b) < tolerance;
}

static int calculate_inverse_transform_matrix(
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
static bool point_in_quad(const Coord2D point, const Coord2D quad[4])
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
