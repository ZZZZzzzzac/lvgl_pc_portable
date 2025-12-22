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

#define ENABLE_AUTO_ROTATION 1 // 0 for mouse, 1 for auto
/* Step 1: Global Resources */

static TransformConfig3D* g_cube = NULL;
static int16_t last_x = 0;
static int16_t last_y = 0;
static bool is_dragging = false;

static lv_obj_t * g_offscreen_root = NULL; // Off-screen root for faces/wrapper
static lv_obj_t * g_wrapper_obj = NULL;
static lv_obj_t * g_display_obj = NULL;
static lv_draw_buf_t * g_snapshot_buf = NULL;

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
    if(!g_cube || !g_wrapper_obj || !g_snapshot_buf)
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
        lv_obj_t* face_obj = g_cube->faces_obj[i];

        if (!is_face_visible(face->vertex_indices, g_cube->vertices_tra))
        continue;

        LV_PROFILER_BEGIN_TAG("not_double_for_loop");
        // Manipulation on OFF-SCREEN objects is safe from active screen invalidation logic
        lv_obj_set_parent(face_obj, g_wrapper_obj);
        lv_obj_remove_flag(face_obj, LV_OBJ_FLAG_HIDDEN);
        lv_obj_align(face_obj, LV_ALIGN_TOP_LEFT, 0, 0);

        // 性能优化：不再缩放face_obj。
        // 使用transform_scale会导致LVGL在渲染时申请巨大的临时图层缓冲(ARGB8888)，造成内存尖峰和抖动。
        // 改为调整wrapper大小以匹配face_obj的原始尺寸，进行1:1快照。
        // 最终的缩放由3D纹理映射的双线性插值自动处理。
        int32_t obj_w = lv_obj_get_width(face_obj);
        int32_t obj_h = lv_obj_get_height(face_obj);

        if(obj_w < 1)
            obj_w = 1;
        if(obj_h < 1)
            obj_h = 1;

        // 调整容器大小以适应内容，由于一开始建立g_wrapper_obj的时候遍历了face_obj的最大尺寸，这里调整后不会出现g_snapshot_buf不够的问题。
        if (lv_obj_get_width(g_wrapper_obj) != obj_w || lv_obj_get_height(g_wrapper_obj) != obj_h)
            lv_obj_set_size(g_wrapper_obj, obj_w, obj_h);

        // Ready to snapshot, update layout to ensure rendering is correct
        lv_obj_update_layout(g_wrapper_obj);

        // not necessary: g_wrapper_obj is guaranteed to be max of all face_obj
        // if (lv_snapshot_reshape_draw_buf(g_wrapper_obj, g_snapshot_buf) != LV_RESULT_OK) {
        //         lv_obj_set_parent(face_obj, g_offscreen_root);
        //         lv_obj_add_flag(face_obj, LV_OBJ_FLAG_HIDDEN);
        //         continue;
        // }

        // 3. Take Snapshot
        lv_snapshot_take_to_draw_buf(g_wrapper_obj, LV_COLOR_FORMAT_RGB565, g_snapshot_buf);

        // 4. Calculate Matrix
        const Coord2D dst_points[4] = {
            g_cube->vertices_pro[face->vertex_indices[1]],
            g_cube->vertices_pro[face->vertex_indices[2]],
            g_cube->vertices_pro[face->vertex_indices[3]],
            g_cube->vertices_pro[face->vertex_indices[0]],
        };
        // 由于face_obj已经被缩放并居中以填满g_wrapper_obj，
        // 我们应该使用g_wrapper_obj的尺寸作为纹理源坐标，
        // 这样可以确保整个snapshot区域被映射到立方体面上。
        int32_t wrapper_w = lv_obj_get_width(g_wrapper_obj);
        int32_t wrapper_h = lv_obj_get_height(g_wrapper_obj);
        const Coord2D src_points[4] = {
            {0, 0}, {0, wrapper_h}, {wrapper_w, wrapper_h}, {wrapper_w, 0}
        };

        float matrix[3][3] = {0};
        // matrix maps screen(dst) -> texture(src)
        calculate_inverse_transform_matrix(src_points, dst_points, matrix);

        // Restore to storage
        lv_obj_set_parent(face_obj, g_offscreen_root);
        lv_obj_add_flag(face_obj, LV_OBJ_FLAG_HIDDEN);

        // 6. Draw the face
        int32_t src_w = g_snapshot_buf->header.w;
        int32_t src_h = g_snapshot_buf->header.h;

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

        // Intersect with the clip/object draw area
        lv_area_t iter_area;
        if(!_lv_area_intersect(&iter_area, &draw_area, &face_area))
            continue;
        LV_PROFILER_END_TAG("not_double_for_loop");
        LV_PROFILER_BEGIN_TAG("double_for_loop");
        LV_LOG_USER("draw area: %dx%d", iter_area.x2 - iter_area.x1, iter_area.y2 - iter_area.y1);

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

                uint16_t color = bilinear_interpolation_draw_buf_rgb565(g_snapshot_buf, u, v);

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

    // Create faces on OFF-SCREEN root
    for (int i = 0; i < 6; i++)
    {
        lv_obj_t * obj = lv_obj_create(g_offscreen_root);
        g_cube->faces_obj[i] = obj;
        // lv_obj_remove_style_all(obj);
        lv_obj_set_style_radius(obj, 0, LV_PART_MAIN);
        // User can change size here to test mixed sizes
        lv_obj_set_size(obj, 200-i*10, 140+i*10);
        lv_obj_set_style_bg_color(obj, lv_color_make(255*(i&0b1), 255*(i&0b10), 255*(i&0b100)), 0);
        lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);

        lv_obj_t * label = lv_label_create(obj);
        lv_obj_set_style_text_font(label, &lv_font_montserrat_20, 0);
        lv_label_set_text(label, "text");
        lv_obj_center(label);

        lv_obj_t * sub_obj1 = lv_obj_create(obj);
        lv_obj_set_size(sub_obj1, 40, 40);
        lv_obj_set_style_bg_color(sub_obj1, lv_color_make(255, 0, 255), 0);
        lv_obj_align(sub_obj1, LV_ALIGN_TOP_LEFT, 5, 5);

        lv_obj_t * btn = lv_button_create(obj);
        lv_obj_set_size(btn, 80, 40);
        lv_obj_align(btn, LV_ALIGN_BOTTOM_MID, 0, -10);
        lv_obj_t * btn_lbl = lv_label_create(btn);
        lv_label_set_text(btn_lbl, "Click Me");
        lv_obj_center(btn_lbl);

        lv_obj_update_layout(obj);
    }

    // Determine max face size to allocate sufficient buffer initially
    int32_t max_w = 1;
    int32_t max_h = 1;
    for(int i = 0; i < 6; i++) {
        if(g_cube->faces_obj[i]) {
            int32_t w = lv_obj_get_width(g_cube->faces_obj[i]);
            int32_t h = lv_obj_get_height(g_cube->faces_obj[i]);
            if(w > max_w) max_w = w;
            if(h > max_h) max_h = h;
        }
    }
    g_wrapper_obj = lv_obj_create(g_offscreen_root);
    lv_obj_remove_style_all(g_wrapper_obj);
    lv_obj_set_size(g_wrapper_obj, max_w, max_h);
    // Initial size will be set later based on max face size
    lv_obj_align(g_wrapper_obj, LV_ALIGN_TOP_LEFT, 0, 0);
    // Make it opaque to optimize memory/perf
    lv_obj_set_style_bg_opa(g_wrapper_obj, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(g_wrapper_obj, lv_color_black(), 0);
    lv_obj_remove_flag(g_wrapper_obj, LV_OBJ_FLAG_HIDDEN); // Make sure it's "visible" on the off-screen

    // Initialize wrapper and buffer with max size to avoid early reallocation
    lv_obj_update_layout(g_wrapper_obj);
    g_snapshot_buf = lv_snapshot_create_draw_buf(g_wrapper_obj, LV_COLOR_FORMAT_RGB565);

#if ENABLE_AUTO_ROTATION
    lv_timer_create(auto_rotate_timer_cb, 30, NULL);
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
