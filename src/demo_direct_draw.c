#include "demo_direct_draw.h"
#include "lvgl/lvgl.h"
#include "lvgl/src/misc/lv_area_private.h"
#include <stdio.h>

/**
 * Demo 1: 直接绘制验证
 * 目标：验证通过 LV_EVENT_DRAW_MAIN 直接操作 layer->draw_buf 像素的可行性。
 */

static void direct_draw_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t * obj = lv_event_get_target(e);

    if(code == LV_EVENT_DRAW_MAIN) {
        /* 获取绘制上下文 */
        lv_layer_t * layer = lv_event_get_layer(e);

        /* 1. 获取图层缓冲区的原始指针 */
        /* 如果 layer->draw_buf 是 NULL，则无法直接绘制 */
        if(layer->draw_buf == NULL) {
            LV_LOG_WARN("Direct Draw: layer->draw_buf is NULL");
            return;
        }

        /* 结构体字段直接访问，参考 lv_draw_buf.h */
        uint8_t * buf_start = layer->draw_buf->data;
        uint32_t stride = layer->draw_buf->header.stride;
        lv_color_format_t cf = (lv_color_format_t)layer->draw_buf->header.cf;

        /* 2. 获取组件的坐标区域 */
        lv_area_t obj_area;
        lv_obj_get_coords(obj, &obj_area);

        /* 3. 获取当前的剪裁区域 (Clip Area) */
        /* 我们只能在 _clip_area 范围内绘制，否则会越界或覆盖不该覆盖的区域 */
        lv_area_t clip_area = layer->_clip_area;

        /* 计算组件与剪裁区域的交集 */
        lv_area_t draw_area;
        if(!lv_area_intersect(&draw_area, &obj_area, &clip_area)) {
            return; /* 无交集，无需绘制 */
        }

        /* 4. 像素遍历 (Scanline) */
        /* 目标：将对象区域涂成红色 */

        uint32_t color_val = 0;
        uint8_t px_size = 0;

        if(cf == LV_COLOR_FORMAT_RGB565 || cf == LV_COLOR_FORMAT_RGB565A8) {
            /* RGB565 Red: 0xF800 (11111 000000 00000) */
            /* 考虑到字节序，可能是低字节在前或高字节在前，这里假设小端序 */
            color_val = 0xF800;
            px_size = 2;
        }
        else if(cf == LV_COLOR_FORMAT_ARGB8888 || cf == LV_COLOR_FORMAT_XRGB8888) {
            /* ARGB Red: 0xFFFF0000 */
            color_val = 0xFFFF0000;
            px_size = 4;
        }
        else if(cf == LV_COLOR_FORMAT_RGB888) {
             /* RGB888 Red */
             color_val = 0xFF0000;
             px_size = 3;
        }
        else {
            LV_LOG_WARN("Direct Draw: Unsupported color format: %d", cf);
            /* 如果不支持，可以在这里返回，避免错误写入 */
            return;
        }

        /* 获取缓冲区相对于屏幕的偏移量 */
        int32_t buf_area_x1 = layer->buf_area.x1;
        int32_t buf_area_y1 = layer->buf_area.y1;

        for (int32_t y = draw_area.y1; y <= draw_area.y2; y++) {
            /* 计算当前行在 buffer 中的起始偏移 */
            /* 屏幕坐标 y 对应 buffer 行索引: y - buf_area_y1 */
            int32_t buf_y = y - buf_area_y1;

            /* 行首指针 */
            uint8_t * row_ptr = buf_start + buf_y * stride;

            /* 计算 x 方向的 buffer 索引范围 */
            int32_t buf_x_start = draw_area.x1 - buf_area_x1;
            int32_t buf_x_end = draw_area.x2 - buf_area_x1;

            if (px_size == 2) {
                /* RGB565 */
                uint16_t * px_ptr = (uint16_t*)row_ptr;
                px_ptr += buf_x_start;
                for (int32_t x = buf_x_start; x <= buf_x_end; x++) {
                    *px_ptr = (uint16_t)color_val;
                    px_ptr++;
                }
            }
            else if (px_size == 4) {
                /* ARGB8888 */
                uint32_t * px_ptr = (uint32_t*)row_ptr;
                px_ptr += buf_x_start;
                for (int32_t x = buf_x_start; x <= buf_x_end; x++) {
                    *px_ptr = color_val;
                    px_ptr++;
                }
            }
             else if (px_size == 3) {
                /* RGB888 */
                uint8_t * px_ptr = row_ptr + buf_x_start * 3;
                for (int32_t x = buf_x_start; x <= buf_x_end; x++) {
                    px_ptr[0] = (color_val >> 0) & 0xFF; // Blue
                    px_ptr[1] = (color_val >> 8) & 0xFF; // Green
                    px_ptr[2] = (color_val >> 16) & 0xFF; // Red
                    px_ptr += 3;
                }
            }
        }

        /* 绘制完成后，需要告诉 LVGL 该区域已经更新（虽然在 DRAW 事件中通常隐含了这一点，但如果是异步或部分更新可能需要） */
        /* 对于软件渲染，DRAW 事件是在 flush 之前的 buffer 填充阶段，所以直接写内存是生效的 */
    }
}

void lv_demo_direct_draw(void)
{
    /* 创建一个基础对象 */
    lv_obj_t * obj = lv_obj_create(lv_screen_active());
    lv_obj_set_size(obj, 200, 150);

    /* 移除默认样式，以免干扰 */
    lv_obj_remove_style_all(obj);
    lv_obj_center(obj);

    /* 添加绘制回调 */
    lv_obj_add_event_cb(obj, direct_draw_event_cb, LV_EVENT_DRAW_MAIN, NULL);

    /* 添加一个 Label 说明 */
    lv_obj_t * label = lv_label_create(lv_screen_active());
    lv_label_set_text(label, "Direct Draw Demo (Red Box)");
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 20);
}
