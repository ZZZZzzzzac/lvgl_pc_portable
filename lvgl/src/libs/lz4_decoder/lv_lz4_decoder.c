#include "lz4.h"
#include "../../draw/lv_image_decoder_private.h"
#include <string.h>
#include "../src/misc/lv_area_private.h"
#include "lv_lz4_decoder.h"
#include "../src/display/lv_display_private.h"

/* ---------------------------------------------------------
   数据结构定义
   --------------------------------------------------------- */
typedef struct {
    uint16_t img_w;
    uint16_t img_h;
    uint16_t block_h;
    uint16_t block_count;
    const uint32_t * offset_table;
    const uint8_t * compressed_data_base;
    lv_draw_buf_t * draw_buf; // 解码用的临时行缓冲
} lz4_decoder_data_t;

/* ---------------------------------------------------------
   1. Info: 获取图片基础信息
   --------------------------------------------------------- */
static lv_result_t decoder_info(lv_image_decoder_t * decoder, lv_image_decoder_dsc_t * dsc, lv_image_header_t * header)
{
    LV_UNUSED(decoder);
    if(dsc->src_type != LV_IMAGE_SRC_VARIABLE) return LV_RESULT_INVALID;

    const lv_image_dsc_t * img_dsc = (const lv_image_dsc_t *)dsc->src;
    const uint8_t * data = img_dsc->data;

    if(data == NULL || memcmp(data, "LZ4B", 4) != 0) return LV_RESULT_INVALID;

    header->w = *(uint16_t *)(data + 4);
    header->h = *(uint16_t *)(data + 6);
    header->cf = LV_COLOR_FORMAT_RGB565;
    header->stride = header->w * 2;
    header->flags = 0; // 关键：不由 LVGL 缓存管理分配

    return LV_RESULT_OK;
}

/* ---------------------------------------------------------
   2. Open: 初始化解码器
   --------------------------------------------------------- */
static lv_result_t decoder_open(lv_image_decoder_t * decoder, lv_image_decoder_dsc_t * dsc)
{
    LV_UNUSED(decoder);
    if(lv_image_src_get_type(dsc->src) != LV_IMAGE_SRC_VARIABLE) return LV_RESULT_INVALID;

    const lv_image_dsc_t * img_dsc = dsc->src;
    const uint8_t * data = img_dsc->data;

    if(memcmp(data, "LZ4B", 4) != 0) return LV_RESULT_INVALID;

    lz4_decoder_data_t * user_data = lv_malloc(sizeof(lz4_decoder_data_t));
    LV_ASSERT_MALLOC(user_data);
    lv_memzero(user_data, sizeof(lz4_decoder_data_t));

    user_data->img_w       = *(uint16_t *)(data + 4);
    user_data->img_h       = *(uint16_t *)(data + 6);
    user_data->block_h     = *(uint16_t *)(data + 8);
    user_data->block_count = *(uint16_t *)(data + 10);
    user_data->offset_table = (const uint32_t *)(data + 12);

    uint32_t header_size = 12 + (user_data->block_count + 1) * sizeof(uint32_t);
    user_data->compressed_data_base = data + header_size;

    // draw_buf 延迟到 get_area 分配
    user_data->draw_buf = NULL;

    dsc->user_data = user_data;
    dsc->decoded = NULL; // 设为 NULL 表示没有解码全图

    return LV_RESULT_OK;
}

/* ---------------------------------------------------------
   3. Get Area: 解码指定块
   --------------------------------------------------------- */
static lv_result_t decoder_get_area(lv_image_decoder_t * decoder, lv_image_decoder_dsc_t * dsc,
                                    const lv_area_t * full_area, lv_area_t * decoded_area)
{
    LV_UNUSED(decoder);
    lz4_decoder_data_t * user_data = dsc->user_data;
    if (!user_data) return LV_RESULT_INVALID;

    /*
       逻辑修正：
       full_area->y1 传入的是 "图像内部的相对Y坐标"。
       例如：请求第0行，就是第0块；请求第16行，就是第1块。
    */
    int32_t block_idx = full_area->y1 / user_data->block_h;

    if(block_idx < 0 || block_idx >= user_data->block_count) {
        LV_LOG_ERROR("[LZ4] Block index out of bounds: %d", (int)block_idx);
        return LV_RESULT_INVALID;
    }

    /* 计算该块在图像中的真实行范围 */
    int32_t y_start = block_idx * user_data->block_h;
    int32_t y_end   = y_start + user_data->block_h - 1;
    if(y_end >= user_data->img_h) y_end = user_data->img_h - 1;

    int32_t current_h = y_end - y_start + 1;

    /* 初始化单块缓冲区 */
    if(user_data->draw_buf == NULL) {
        user_data->draw_buf = lv_draw_buf_create(
            user_data->img_w,
            user_data->block_h,
            LV_COLOR_FORMAT_RGB565,
            user_data->img_w * 2
        );
        if(!user_data->draw_buf) return LV_RESULT_INVALID;
        dsc->decoded = user_data->draw_buf;
    }

    /* LZ4 解压 */
    uint32_t abs_offset = user_data->offset_table[block_idx];
    uint32_t comp_size  = user_data->offset_table[block_idx + 1] - abs_offset;
    const lv_image_dsc_t * img_dsc = dsc->src;

    int result = LZ4_decompress_safe(
        (const char *)(img_dsc->data + abs_offset),
        (char *)user_data->draw_buf->data,
        comp_size,
        user_data->img_w * current_h * 2
    );

    if(result < 0) {
        LV_LOG_ERROR("[LZ4] Decompress fail block %d code %d", (int)block_idx, result);
        return LV_RESULT_INVALID;
    }

    /* 返回区域：告知 LVGL 这块数据对应图像的什么位置 */
    decoded_area->x1 = 0;
    decoded_area->x2 = user_data->img_w - 1;
    decoded_area->y1 = y_start;
    decoded_area->y2 = y_end;

    return LV_RESULT_OK;
}

/* ---------------------------------------------------------
   4. Close: 清理
   --------------------------------------------------------- */
static void decoder_close(lv_image_decoder_t * decoder, lv_image_decoder_dsc_t * dsc)
{
    LV_UNUSED(decoder);
    lz4_decoder_data_t * user_data = dsc->user_data;
    if(user_data) {
        if(user_data->draw_buf) {
            lv_draw_buf_destroy(user_data->draw_buf);
        }
        lv_free(user_data);
    }
}

/* ---------------------------------------------------------
   5. 初始化注册
   --------------------------------------------------------- */
void lv_lz4_decoder_init(void)
{
    lv_image_decoder_t * dec = lv_image_decoder_create();
    if(dec) {
        lv_image_decoder_set_info_cb(dec, decoder_info);
        lv_image_decoder_set_open_cb(dec, decoder_open);
        lv_image_decoder_set_get_area_cb(dec, decoder_get_area);
        lv_image_decoder_set_close_cb(dec, decoder_close);
        dec->name = "LZ4";
    }
}
static void img_custom_draw_event(lv_event_t * e)
{
    /* 1. 基础事件校验 */
    if(lv_event_get_code(e) != LV_EVENT_DRAW_MAIN) return;

    lv_obj_t * obj = lv_event_get_target(e);

    /* 获取图片源 (根据你之前代码，你是放在 user_data 里的) */
    const void * src = lv_obj_get_user_data(obj);
    if(!src) return;

    /*
       2. 打开解码器进行身份校验
       利用 LVGL 的机制，如果能打开且名字是 "LZ4"，说明这是我们需要处理的对象。
       如果是 PNG/JPG，名字会不匹配，直接退出让系统处理。
    */
    lv_image_decoder_dsc_t decoder_dsc;
    if(lv_image_decoder_open(&decoder_dsc, src, NULL) != LV_RESULT_OK) {
        return; // 打开失败
    }

    /* 校验解码器名称，确保只处理 LZ4 */
    if(strcmp(decoder_dsc.decoder->name, "LZ4") != 0) {
        lv_image_decoder_close(&decoder_dsc);
        return; // 不是 LZ4，交给系统默认绘制
    }

    /* 类型转换，准备开始绘制 */
    lz4_decoder_data_t * img_data = (lz4_decoder_data_t *)decoder_dsc.user_data;
    lv_layer_t * layer = lv_event_get_layer(e);

    /*
       3. 计算可见区域 (优化核心)
       先算出图片在屏幕上的绝对位置，再和当前剪裁区(Clip)取交集。
    */
    lv_area_t img_abs_area;
    lv_obj_get_coords(obj, &img_abs_area);

    lv_area_t visible_area;
    if(!lv_area_intersect(&visible_area, &img_abs_area, &layer->_clip_area)) {
        /* 完全不可见 */
        lv_image_decoder_close(&decoder_dsc);
        lv_event_stop_processing(e); // 我们接管了但不需要画
        return;
    }

    /*
       4. 计算需要解码的 Block 范围 (性能优化)
       仅循环处理涉及到的块，避免全量循环。
    */
    int32_t rel_y1 = visible_area.y1 - img_abs_area.y1;
    int32_t rel_y2 = visible_area.y2 - img_abs_area.y1;

    int32_t start_block = rel_y1 / img_data->block_h;
    int32_t end_block   = rel_y2 / img_data->block_h;

    if(start_block < 0) start_block = 0;
    if(end_block >= img_data->block_count) end_block = img_data->block_count - 1;

    /*
       5. 获取 Layer 缓冲区的绝对偏移量 (修复崩溃的核心)
       buf_x_off: 针对滚动条或局部小区域刷新时非常重要
       buf_y_off: 针对分块渲染时重要
    */
    int32_t buf_x_off = layer->buf_area.x1;
    int32_t buf_y_off = layer->buf_area.y1;

    /* ---------------- 绘制循环 ---------------- */
    for(int b = start_block; b <= end_block; b++) {

        /* 构造请求区域 (图像相对坐标) */
        lv_area_t req_area;
        req_area.x1 = 0;
        req_area.x2 = img_data->img_w - 1;
        req_area.y1 = b * img_data->block_h;
        req_area.y2 = req_area.y1 + img_data->block_h - 1;

        /* 解码该块 */
        lv_area_t decoded_area_info;
        if(lv_image_decoder_get_area(&decoder_dsc, &req_area, &decoded_area_info) != LV_RESULT_OK) {
            continue;
        }

        /* 计算当前块在屏幕上的绝对位置 */
        lv_area_t block_abs_area;
        block_abs_area.x1 = img_abs_area.x1;
        block_abs_area.x2 = img_abs_area.x2;
        block_abs_area.y1 = img_abs_area.y1 + req_area.y1;
        block_abs_area.y2 = img_abs_area.y1 + req_area.y2;

        /* 计算 块 与 可见区 的交集 (即实际要画的那一部分) */
        lv_area_t draw_area;
        if(!lv_area_intersect(&draw_area, &block_abs_area, &visible_area)) {
            continue;
        }

        /*
           6. 坐标转换 (Src vs Dest)
        */

        // Src: 相对解码缓冲区的坐标
        // (绘制区绝对坐标 - 块绝对起点)
        lv_area_t src_area;
        src_area.x1 = draw_area.x1 - block_abs_area.x1;
        src_area.y1 = draw_area.y1 - block_abs_area.y1;
        src_area.x2 = src_area.x1 + lv_area_get_width(&draw_area) - 1;
        src_area.y2 = src_area.y1 + lv_area_get_height(&draw_area) - 1;

        // Dest: 相对 Layer 缓冲区的坐标
        // (绘制区绝对坐标 - Layer缓冲区起点)
        // ★★★ 这里的 X 减法修复了滚动条刷新的崩溃 ★★★
        lv_area_t dest_area_local;
        dest_area_local.x1 = draw_area.x1 - buf_x_off;
        dest_area_local.y1 = draw_area.y1 - buf_y_off;
        dest_area_local.x2 = draw_area.x2 - buf_x_off;
        dest_area_local.y2 = draw_area.y2 - buf_y_off;

        /*
           7. 终极安全校验
        */
        // 确保不超出 Layer 缓冲区的物理边界
        if(dest_area_local.x1 < 0) dest_area_local.x1 = 0;
        if(dest_area_local.y1 < 0) dest_area_local.y1 = 0;
        if(dest_area_local.x2 >= layer->draw_buf->header.w) dest_area_local.x2 = layer->draw_buf->header.w - 1;
        if(dest_area_local.y2 >= layer->draw_buf->header.h) dest_area_local.y2 = layer->draw_buf->header.h - 1;

        // 防止坐标倒回 (x1 > x2 或 y1 > y2)
        if(dest_area_local.x1 > dest_area_local.x2 || dest_area_local.y1 > dest_area_local.y2) {
            continue;
        }

        // 确保源和目标尺寸一致 (取最小值，防止裁剪导致的 1px 误差)
        int32_t w = LV_MIN(lv_area_get_width(&src_area), lv_area_get_width(&dest_area_local));
        int32_t h = LV_MIN(lv_area_get_height(&src_area), lv_area_get_height(&dest_area_local));

        if(w <= 0 || h <= 0) continue;

        // 重设右下角坐标以匹配计算出的宽高
        src_area.x2 = src_area.x1 + w - 1;
        src_area.y2 = src_area.y1 + h - 1;
        dest_area_local.x2 = dest_area_local.x1 + w - 1;
        dest_area_local.y2 = dest_area_local.y1 + h - 1;

        /* 执行拷贝 */
        lv_draw_buf_copy(layer->draw_buf, &dest_area_local, (lv_draw_buf_t *)decoder_dsc.decoded, &src_area);
    }

    /* 8. 收尾工作 */
    lv_image_decoder_close(&decoder_dsc);

    /*
       阻止事件冒泡：
       告诉 LVGL "我已经把这个部件画好了，不要调用默认的图片绘制函数了"
       否则系统会尝试再次解码并覆盖你的绘制结果。
    */
    lv_event_stop_processing(e);
}


void lv_lz4_image_set_custom_draw(lv_obj_t * img)
{
    lv_obj_add_event_cb(img, img_custom_draw_event, LV_EVENT_DRAW_MAIN, NULL);
}
