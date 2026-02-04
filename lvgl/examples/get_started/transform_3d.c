#include <stdlib.h>
#include <stdio.h>
#include "transform_3d.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

#include "transform_3d_helper.h"
#include "lvgl/src/draw/snapshot/lv_snapshot.h"
#include "lvgl/src/misc/lv_area_private.h"
#include "basic_math.h"
#include "../src/C_array.h"

#define ENABLE_AUTO_ROTATION 1 // 0 for mouse, 1 for auto
/* Step 1: Global Resources */
void test_colors(lv_obj_t *parent)
{
    // 创建多个测试块
    int y = 0;

    // 1. 纯红色 - RGB
    lv_obj_t *r1 = lv_obj_create(parent);
    lv_obj_set_size(r1, 50, 20);
    lv_obj_set_style_bg_color(r1, lv_color_make(255, 0, 0), 0);
    lv_obj_set_pos(r1, 0, y); y += 25;

    // 2. 纯红色 - 16进制
    lv_obj_t *r2 = lv_obj_create(parent);
    lv_obj_set_size(r2, 50, 20);
    lv_obj_set_style_bg_color(r2, lv_color_hex(0xFF0000), 0);
    lv_obj_set_pos(r2, 0, y); y += 25;

    // 3. 纯绿色 - 作为对比
    lv_obj_t *g1 = lv_obj_create(parent);
    lv_obj_set_size(g1, 50, 20);
    lv_obj_set_style_bg_color(g1, lv_color_make(0, 255, 0), 0);
    lv_obj_set_pos(g1, 0, y); y += 25;

    // 4. 纯蓝色
    lv_obj_t *b1 = lv_obj_create(parent);
    lv_obj_set_size(b1, 50, 20);
    lv_obj_set_style_bg_color(b1, lv_color_make(0, 0, 255), 0);
    lv_obj_set_pos(b1, 0, y); y += 25;

    // 5. 白色
    lv_obj_t *w1 = lv_obj_create(parent);
    lv_obj_set_size(w1, 50, 20);
    lv_obj_set_style_bg_color(w1, lv_color_white(), 0);
    lv_obj_set_pos(w1, 0, y); y += 25;

    // 6. 黑色
    lv_obj_t *bl1 = lv_obj_create(parent);
    lv_obj_set_size(bl1, 50, 20);
    lv_obj_set_style_bg_color(bl1, lv_color_black(), 0);
    lv_obj_set_pos(bl1, 0, y);
}

static TransformConfig3D* g_cube = NULL;
static int16_t last_x = 0;
static int16_t last_y = 0;
static bool is_dragging = false;

#define FACE_SNAPSHOT_SIZE 100

static lv_obj_t * g_offscreen_root = NULL; // Off-screen root for faces/wrapper
static lv_obj_t * g_wrapper_obj = NULL;
static lv_obj_t * g_display_obj = NULL;
static lv_draw_buf_t * g_snapshot_bufs[6] = {0};

static TransformConfig3D* transform_config_3d_create(
    uint32_t canvas_width, uint32_t canvas_height)
{
    TransformConfig3D* config = (TransformConfig3D*)lv_malloc(sizeof(TransformConfig3D));
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
    config->faces_ptr = (CoordFace*)lv_malloc(sizeof(CoordFace) * config->faces_len);
    config->faces_obj = (lv_obj_t**)lv_malloc(sizeof(lv_obj_t*) * config->faces_len);
    config->faces_mat = (lv_matrix_t*)lv_malloc(sizeof(lv_matrix_t) * config->faces_len);
    config->vertices_ptr = (Coord3D*)lv_malloc(sizeof(Coord3D) * config->vertices_len);
    config->vertices_pro = (Coord2D*)lv_malloc(sizeof(Coord2D) * config->vertices_len);
    config->vertices_tra = (Coord3D*)lv_malloc(sizeof(Coord3D) * config->vertices_len);
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
    config->vertices_ptr = (Coord3D*)lv_malloc(sizeof(Coord3D) * config->vertices_len);
    config->vertices_pro = (Coord2D*)lv_malloc(sizeof(Coord2D) * config->vertices_len);
    config->vertices_tra = (Coord3D*)lv_malloc(sizeof(Coord3D) * config->vertices_len);
    config->faces_ptr = (CoordFace*)lv_malloc(sizeof(CoordFace) * n_sides);
    config->faces_mat = (lv_matrix_t*)lv_malloc(sizeof(lv_matrix_t) * n_sides);
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

    // 计算底部和顶部顶点的坐标
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
    // 定义棱柱的侧面
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
    return config;
}

static int apply_transformations_3d()
{
    if (!g_cube) {
        return -1; // 参数错误
    }

    if (!g_cube->vertices_ptr || g_cube->vertices_len == 0) {
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
    if (!(is_equal(g_cube->scale[0], 1.0f, 1e-6f) &&
          is_equal(g_cube->scale[1], 1.0f, 1e-6f) &&
          is_equal(g_cube->scale[2], 1.0f, 1e-6f)))
    {
        create_scaling_matrix_3x3(g_cube->scale[0], g_cube->scale[1], g_cube->scale[2], matrix_b);
        matrix_multiply_3x3(matrix_in, matrix_b, matrix_out);
        matrix_tmp = matrix_in;
        matrix_in = matrix_out;
        matrix_out = matrix_tmp;
    }

    // 应用旋转
    if (!is_equal(g_cube->rotation_deg[0], 0, 1e-6f))
    {
        create_rotation_matrix_x_3x3(g_cube->rotation_deg[0], matrix_b);
        matrix_multiply_3x3(matrix_in, matrix_b, matrix_out);
        matrix_tmp = matrix_in;
        matrix_in = matrix_out;
        matrix_out = matrix_tmp;
    }

    if (!is_equal(g_cube->rotation_deg[1], 0, 1e-6f))
    {
        create_rotation_matrix_y_3x3(g_cube->rotation_deg[1], matrix_b);
        matrix_multiply_3x3(matrix_in, matrix_b, matrix_out);
        matrix_tmp = matrix_in;
        matrix_in = matrix_out;
        matrix_out = matrix_tmp;
    }

    if (!is_equal(g_cube->rotation_deg[2], 0, 1e-6f))
    {
        create_rotation_matrix_z_3x3(g_cube->rotation_deg[2], matrix_b);
        matrix_multiply_3x3(matrix_in, matrix_b, matrix_out);
        matrix_tmp = matrix_in;
        matrix_in = matrix_out;
        matrix_out = matrix_tmp;
    }

    // 应用变换到每个顶点
    for (uint32_t i = 0; i < g_cube->vertices_len; i++) {
        const Coord3D* vertex = &g_cube->vertices_ptr[i];
        float vertex_array[3] = {vertex->x, vertex->y, vertex->z};
        float transformed_array[3] = {0};

        // 应用旋转变换
        matrix_vector_multiply_3x3(matrix_a, vertex_array, transformed_array);

        // 存储变换后的顶点
        g_cube->vertices_tra[i] = (Coord3D){
            transformed_array[0],
            transformed_array[1],
            transformed_array[2]
        };

        // 简单正交投影 (忽略透视)
        float offset_x = 0.0f;
        float offset_y = 0.0f;
        if(g_display_obj) {
            offset_x = (lv_obj_get_width(g_display_obj) - g_cube->canvas_width) * 0.5f;
            offset_y = (lv_obj_get_height(g_display_obj) - g_cube->canvas_height) * 0.5f;
        }

        float screen_x = (transformed_array[0] + 1.0f) * 0.5f * g_cube->canvas_width + offset_x;
        float screen_y = (1.0f - transformed_array[1]) * 0.5f * g_cube->canvas_height + offset_y;

        // 存储投影后的顶点
        g_cube->vertices_pro[i] = (Coord2D){screen_x, screen_y};
    }

    return 0; // 成功
}

/**
 * Custom Draw Event:
 * Iterates over visible faces, takes snapshot of each, and draws it transformed.
 */
static void snapshot_draw_event_cb(lv_event_t * e)
{
    LV_PROFILER_BEGIN_TAG("snapshot_draw_event_cb");
    lv_event_code_t code = lv_event_get_code(e);
    if(code != LV_EVENT_DRAW_MAIN)
        return;
    if(!g_cube || !g_wrapper_obj || !g_snapshot_bufs[0])
        return;

    lv_layer_t * layer = lv_event_get_layer(e);
    lv_obj_t * display_obj = lv_event_get_target(e);

    /* Get the raw pointer to the screen's draw buffer */
    uint8_t * dest_buf_start = layer->draw_buf->data;
    uint32_t dest_stride = layer->draw_buf->header.stride;
    lv_area_t clip_area = layer->_clip_area;
    lv_area_t obj_coords;
    lv_area_t draw_area;

    /* Calculate the intersection of the object and the clip area */
    lv_obj_get_coords(display_obj, &obj_coords);
    if(!lv_area_intersect(&draw_area, &obj_coords, &clip_area))
        return;

    // Loop through faces
    for (uint32_t i = 0; i < g_cube->faces_len; ++i)
    {
        const CoordFace* face = &g_cube->faces_ptr[i];

        if (!is_face_visible(face->vertex_indices, g_cube->vertices_tra))
            continue;

        lv_draw_buf_t * snapshot_buf = g_snapshot_bufs[i];
        if(!snapshot_buf)
            continue;

        LV_PROFILER_BEGIN_TAG("TAG4");
        // 4. Calculate Matrix
        const Coord2D dst_points[4] = {
            g_cube->vertices_pro[face->vertex_indices[1]],
            g_cube->vertices_pro[face->vertex_indices[2]],
            g_cube->vertices_pro[face->vertex_indices[3]],
            g_cube->vertices_pro[face->vertex_indices[0]],
        };

        // 100x100 fixed size
        const Coord2D src_points[4] = {
            {0, 0}, {0, FACE_SNAPSHOT_SIZE}, {FACE_SNAPSHOT_SIZE, FACE_SNAPSHOT_SIZE}, {FACE_SNAPSHOT_SIZE, 0}
        };
        LV_PROFILER_END_TAG("TAG4");
        LV_PROFILER_BEGIN_TAG("TAG5");
        float matrix[3][3] = {0};
        // matrix maps screen(dst) -> texture(src)
        calculate_inverse_transform_matrix(src_points, dst_points, matrix);
        LV_PROFILER_END_TAG("TAG5");

        LV_PROFILER_BEGIN_TAG("TAG7");
        // 6. Draw the face
        int32_t src_w = snapshot_buf->header.w;
        int32_t src_h = snapshot_buf->header.h;

        // Optimization: Calculate bounding box of the face to limit the loop
        float f_min_x = dst_points[0].x;
        float f_max_x = dst_points[0].x;
        float f_min_y = dst_points[0].y;
        float f_max_y = dst_points[0].y;

        for(int k = 1; k < 4; k++) {
            if(dst_points[k].x < f_min_x) f_min_x = dst_points[k].x;
            if(dst_points[k].x > f_max_x) f_max_x = dst_points[k].x;
            if(dst_points[k].y < f_min_y) f_min_y = dst_points[k].y;
            if(dst_points[k].y > f_max_y) f_max_y = dst_points[k].y;
        }

        lv_area_t face_area;
        face_area.x1 = (int32_t)floorf(f_min_x);
        face_area.y1 = (int32_t)floorf(f_min_y);
        face_area.x2 = (int32_t)ceilf(f_max_x);
        face_area.y2 = (int32_t)ceilf(f_max_y);
        LV_PROFILER_END_TAG("TAG7");

        // Intersect with the clip/object draw area
        lv_area_t iter_area;
        if(!_lv_area_intersect(&iter_area, &draw_area, &face_area))
            continue;

        LV_PROFILER_BEGIN_TAG("double_for_loop");
        // LV_LOG_USER("draw area: %dx%d", iter_area.x2 - iter_area.x1, iter_area.y2 - iter_area.y1);

        for(int32_t draw_y = iter_area.y1; draw_y <= iter_area.y2; draw_y++) {
            uint32_t dest_y_idx = (draw_y - layer->buf_area.y1);
            uint8_t * dest_row = dest_buf_start + (dest_y_idx * dest_stride);

            float y_local = (float)(draw_y - obj_coords.y1);

            for(int32_t draw_x = iter_area.x1; draw_x <= iter_area.x2; draw_x++) {
                float x_local = (float)(draw_x - obj_coords.x1);

                // Inverse Matrix: screen(x,y) -> texture(u,v)
                float u = matrix[0][0] * x_local + matrix[0][1] * y_local + matrix[0][2];
                float v = matrix[1][0] * x_local + matrix[1][1] * y_local + matrix[1][2];

                if(u < 0 || u >= src_w || v < 0 || v >= src_h)
                    continue;

                uint16_t color = bilinear_interpolation_draw_buf_rgb565(snapshot_buf, u, v);

                uint32_t dest_x_idx = (draw_x - layer->buf_area.x1);
                uint16_t * dst_px = (uint16_t*)(dest_row + dest_x_idx * 2); // 2=RGB565

                *dst_px = color;
            }
        }
        LV_PROFILER_END_TAG("double_for_loop");
    }
    LV_PROFILER_END_TAG("snapshot_draw_event_cb");
}


static void auto_rotate_timer_cb(lv_timer_t * timer)
{
    if(!g_cube) return;
    g_cube->rotation_deg[1] += 1.0f; // Rotate Y
    g_cube->rotation_deg[0] += 0.5f; // Rotate X
    LV_PROFILER_BEGIN_TAG("apply_transformations_3d");
    apply_transformations_3d();
    LV_PROFILER_END_TAG("apply_transformations_3d");
    lv_obj_invalidate(g_display_obj);
}

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
                    apply_transformations_3d();

                    // Trigger redraw
                    lv_obj_invalidate(g_display_obj);

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

    // Create off-screen root for faces and wrapper
    g_offscreen_root = lv_obj_create(NULL);
    lv_obj_remove_style_all(g_offscreen_root);

    g_display_obj = lv_obj_create(lv_screen_active());
    lv_obj_remove_style_all(g_display_obj);
    lv_obj_set_size(g_display_obj, DISP_HOR_RES, DISP_VER_RES);
    lv_obj_center(g_display_obj);
    lv_obj_set_style_bg_opa(g_display_obj, LV_OPA_COVER, 0);

    // Register draw callback
    lv_obj_add_event_cb(g_display_obj, snapshot_draw_event_cb, LV_EVENT_DRAW_MAIN, NULL);

    g_wrapper_obj = lv_obj_create(g_offscreen_root);
    lv_obj_remove_style_all(g_wrapper_obj);
    lv_obj_set_size(g_wrapper_obj, FACE_SNAPSHOT_SIZE, FACE_SNAPSHOT_SIZE);
    lv_obj_align(g_wrapper_obj, LV_ALIGN_TOP_LEFT, 0, 0);
    // Make it opaque to optimize memory/perf
    lv_obj_set_style_bg_opa(g_wrapper_obj, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(g_wrapper_obj, lv_color_black(), 0);
    lv_obj_remove_flag(g_wrapper_obj, LV_OBJ_FLAG_HIDDEN);

    // Create faces on OFF-SCREEN root
    for (int i = 0; i < 6; i++)
    {
        lv_obj_t * obj = lv_obj_create(g_offscreen_root);
        g_cube->faces_obj[i] = obj;
        // lv_obj_remove_style_all(obj);
        lv_obj_set_scrollbar_mode(obj, LV_SCROLLBAR_MODE_OFF);
        lv_obj_set_style_radius(obj, 0, LV_PART_MAIN);
        // User can change size here to test mixed sizes
        lv_obj_set_size(obj, FACE_SNAPSHOT_SIZE, FACE_SNAPSHOT_SIZE); // 缩小版的各个页面
        lv_obj_set_style_bg_color(obj, lv_color_make(0, 0, 0), 0);
        lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);

        lv_obj_t * hrs = lv_image_create(obj);
        lv_image_set_src(hrs, PIC_ADDR(HRS_8));
        lv_img_set_zoom(hrs,100);
        lv_obj_align(hrs, LV_ALIGN_CENTER, -27, -30);
        // lv_obj_set_pos(hrs, -30, -25);

        lv_obj_t * mmHg = lv_image_create(obj);
        lv_img_set_zoom(mmHg,150);
        lv_image_set_src(mmHg, PIC_ADDR(MMHG_F));
        lv_obj_align(mmHg, LV_ALIGN_CENTER, 8, -35);
        // lv_obj_set_pos(mmHg, 20, -6);

        lv_obj_t * label1 = lv_label_create(obj);
        lv_obj_set_style_text_font(label1, &lv_font_montserrat_10, 0);
        lv_obj_set_style_text_color(label1, lv_color_hex(0xffffff),0);
        lv_label_set_text_fmt(label1, "%d",120+i);
        lv_obj_align(label1, LV_ALIGN_CENTER, 0, -25);
        // lv_obj_set_pos(label1, 32, 8);
        lv_obj_t * label2 = lv_label_create(obj);
        lv_obj_set_style_text_font(label2, &lv_font_montserrat_10, 0);
        lv_obj_set_style_text_color(label2, lv_color_hex(0xffffff),0);
        lv_label_set_text_fmt(label2, "%d",85+i);
        lv_obj_align(label2, LV_ALIGN_CENTER, 20, -25);
        // lv_obj_set_pos(label2, 55, 8);

        lv_obj_t * label3 = lv_label_create(obj);
        lv_obj_set_style_text_font(label3, &lv_font_montserrat_8, 0);
        lv_obj_set_style_text_color(label3, lv_color_hex(0xffffff),0);
        lv_label_set_text(label3, "2 minutes ago:");
        lv_obj_align(label3, LV_ALIGN_CENTER, -15, -10);
        // lv_obj_set_pos(label3, -8, 20);

        lv_obj_t * label4 = lv_label_create(obj);
        lv_obj_set_style_text_font(label4, &lv_font_montserrat_8, 0);
        lv_obj_set_style_text_color(label4, lv_color_hex(0xffffff),0);
        lv_label_set_text_fmt(label4, "%d/%d",120+i,85+i);
        lv_obj_align(label4, LV_ALIGN_CENTER, 32, -10);
        // lv_obj_set_pos(label4, 55, 20);

        lv_obj_t * sub_obj1 = lv_obj_create(obj);
        lv_obj_set_scrollbar_mode(sub_obj1, LV_SCROLLBAR_MODE_OFF);
        lv_obj_set_size(sub_obj1, 80, 20);
        lv_obj_set_style_radius(sub_obj1, 5, 0);
        lv_obj_set_style_bg_color(sub_obj1, lv_color_make(58, 61, 58), 0);
        // lv_obj_set_style_bg_opa(sub_obj1, LV_OPA_70, 0);
        lv_obj_set_style_border_width(sub_obj1, 0, 0);
        lv_obj_align(sub_obj1, LV_ALIGN_CENTER, 0, 8);

        lv_obj_t * sub_obj2 = lv_obj_create(obj);
        lv_obj_set_scrollbar_mode(sub_obj2, LV_SCROLLBAR_MODE_OFF);
        lv_obj_set_size(sub_obj2, 80, 20);
        lv_obj_set_style_radius(sub_obj2, 5, 0);
        lv_obj_set_style_bg_color(sub_obj2, lv_color_make(58, 61, 58), 0);
        // lv_obj_set_style_bg_opa(sub_obj2, LV_OPA_70, 0);
        lv_obj_set_style_border_width(sub_obj2, 0, 0);
        lv_obj_align(sub_obj2, LV_ALIGN_CENTER, 0, 33);



        lv_obj_t * spo2 = lv_image_create(obj);
        lv_img_set_zoom(spo2,180);
        lv_image_set_src(spo2, PIC_ADDR(SPO2));
        lv_obj_align(spo2, LV_ALIGN_CENTER, -25, 7);

        lv_obj_t * label5 = lv_label_create(obj);
        lv_obj_set_style_text_font(label5, &lv_font_montserrat_8, 0);
        lv_label_set_text_fmt(label5, "%d%%",91+i);
        lv_obj_set_style_text_color(label5, lv_color_hex(0xffffff),0);
        lv_obj_set_pos(label5, 30, 37);
        lv_obj_align(label5, LV_ALIGN_CENTER, 0, 7);

        lv_obj_t * label6 = lv_label_create(obj);
        lv_obj_set_style_text_font(label6, &lv_font_montserrat_8, 0);
        lv_label_set_text(label6, "spo2");
        lv_obj_set_style_text_color(label6, lv_color_hex(0x00ff2f),0);
        lv_obj_set_pos(label6, 52, 37);
        lv_obj_align(label6, LV_ALIGN_CENTER, 25, 7);
        // /*****************************/

        lv_obj_t * bp = lv_image_create(obj);
        lv_img_set_zoom(bp,180);
        lv_image_set_src(bp, PIC_ADDR(BP));
        lv_obj_set_pos(bp, -3, 56);
        lv_obj_align(bp, LV_ALIGN_CENTER, -25, 33);

        lv_obj_t * label7 = lv_label_create(obj);
        lv_obj_set_style_text_font(label7, &lv_font_montserrat_8, 0);
        lv_obj_set_style_text_color(label7, lv_color_hex(0xffffff),0);
        lv_label_set_text_fmt(label7, "0%d",91+i);
        lv_obj_set_pos(label7, 30, 62);
        lv_obj_align(label7, LV_ALIGN_CENTER, 0, 33);

        lv_obj_t * label8 = lv_label_create(obj);
        lv_obj_set_style_text_font(label8, &lv_font_montserrat_8, 0);
        lv_label_set_text(label8, "bpm");
        lv_obj_set_style_text_color(obj, lv_color_hex(0xff4545), 0);
        lv_obj_set_pos(label8, 52, 62);
        lv_obj_align(label8, LV_ALIGN_CENTER, 25, 33);

        /*****************************/

        lv_obj_update_layout(obj);

        /* --- New logic: Scale and Snapshot immediately --- */
        // 1. Set parent to wrapper and resize to wrapper size
        lv_obj_set_parent(obj, g_wrapper_obj);
        lv_obj_remove_flag(obj, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_size(obj, FACE_SNAPSHOT_SIZE, FACE_SNAPSHOT_SIZE);
        lv_obj_align(obj, LV_ALIGN_TOP_LEFT, 0, 0);

        // 2. Ensure layout is updated before snapshot
        lv_obj_update_layout(g_wrapper_obj);
        lv_obj_update_layout(obj);

        // 3. Take Snapshot
        g_snapshot_bufs[i] = lv_snapshot_create_draw_buf(g_wrapper_obj, LV_COLOR_FORMAT_RGB565);
        if(g_snapshot_bufs[i]) {
            lv_snapshot_take_to_draw_buf(g_wrapper_obj, LV_COLOR_FORMAT_RGB565, g_snapshot_bufs[i]);
        }

        // 4. Restore (optional, but good for cleanup if needed later)
        // Since we only use snapshots now, we can hide/move them back or just leave them.
        // Moving back to offscreen root to keep wrapper clean for next face.
        lv_obj_set_parent(obj, g_offscreen_root);
        lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
    }

#if ENABLE_AUTO_ROTATION
    lv_timer_create(auto_rotate_timer_cb, 30, NULL); // 30ms trigger rotation
#else
// 创建透明的鼠标事件捕获层
    lv_obj_t * mouse_layer = lv_obj_create(lv_screen_active());
    lv_obj_remove_style_all(mouse_layer);
    lv_obj_set_size(mouse_layer, DISP_HOR_RES, DISP_VER_RES); // Full screen
    lv_obj_set_pos(mouse_layer, 0, 0);
    lv_obj_set_style_bg_opa(mouse_layer, LV_OPA_TRANSP, 0); // Ensure transparency
    lv_obj_clear_flag(mouse_layer, LV_OBJ_FLAG_SCROLLABLE);

    // 使能点击并添加事件回调
    lv_obj_add_flag(mouse_layer, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(mouse_layer, LV_OBJ_FLAG_SCROLL_ON_FOCUS);
    lv_obj_add_event_cb(mouse_layer, mouse_handler, LV_EVENT_ALL, NULL);
#endif

    g_cube->rotation_deg[0] = -10.0f;
    g_cube->rotation_deg[1] = 40.0f;
    g_cube->rotation_deg[2] = 0.0f;

    lv_obj_update_layout(g_display_obj); // Ensure size is valid for apply_transformations
    apply_transformations_3d();
    lv_obj_invalidate(g_display_obj); // Force initial draw
}
