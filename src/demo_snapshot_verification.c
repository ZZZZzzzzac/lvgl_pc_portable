#include "demo_snapshot_verification.h"
#include "lvgl/src/misc/lv_area_private.h"
#include <stdio.h>

/*
 * SWITCH: Toggle between Snapshot+DirectDraw path and Standard Widget path.
 * 1 = Use Snapshot + Direct Draw (Target Solution, saves RAM of widget structure but uses Snapshot buffer)
 * 0 = Use Standard Widget (Baseline, uses RAM of widget structure tree)
 */
#define DEMO_USE_SNAPSHOT_PATH 1

/*
 * SWITCH: Down-scale the snapshot to save RAM?
 * 1 = Scale widget to 50% before snapshot (200x200 -> 100x100). Saves ~75% RAM.
 * 0 = 1:1 Snapshot (200x200 -> 200x200).
 */
#define DEMO_SNAPSHOT_DOWN_SCALE 1

/* Global snapshot buffer reference */
static lv_draw_buf_t * g_snapshot_buf = NULL;

/**
 * Custom Draw Event:
 * Directly copies pixels from the snapshot buffer to the screen's draw buffer.
 */
static void snapshot_draw_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if(code != LV_EVENT_DRAW_MAIN) return;

    if(!g_snapshot_buf) return;

    lv_layer_t * layer = lv_event_get_layer(e);
    lv_obj_t * obj = lv_event_get_target(e);

    /* We assume the object size matches the snapshot size for this demo 1:1 mapping */
    int32_t h = g_snapshot_buf->header.h;

    /* Get the raw pointer to the screen's draw buffer */
    uint8_t * dest_buf_start = layer->draw_buf->data;
    uint32_t dest_stride = layer->draw_buf->header.stride;

    /* Snapshot buffer details */
    uint8_t * src_buf_start = g_snapshot_buf->data;
    uint32_t src_stride = g_snapshot_buf->header.stride;

    /* Coordinates of the object on screen */
    lv_area_t obj_coords;
    lv_obj_get_coords(obj, &obj_coords);

    /* Clip area (the area currently being updated) */
    lv_area_t clip_area = layer->_clip_area;

    /* Calculate the intersection of the object and the clip area */
    lv_area_t draw_area;
    if(!_lv_area_intersect(&draw_area, &obj_coords, &clip_area)) return;

    /*
       Prepare for 16-bit (RGB565) copy.
       We assume LV_COLOR_DEPTH 16 from lv_conf.h check.
    */
    int px_size = 2; // 16-bit = 2 bytes

    /* Loop through the intersecting area */
    for(int32_t y = draw_area.y1; y <= draw_area.y2; y++) {
        /* Screen buffer offset calculation */
        uint32_t dest_y_idx = (y - layer->buf_area.y1);
        uint8_t * dest_row = dest_buf_start + (dest_y_idx * dest_stride);

        /* Source coordinates (Snapshot is 0-based relative to itself) */
        int32_t src_y_local = y - obj_coords.y1;

        if(src_y_local < 0 || src_y_local >= h) continue;

        uint8_t * src_row = src_buf_start + (src_y_local * src_stride);

        for(int32_t x = draw_area.x1; x <= draw_area.x2; x++) {

            int32_t src_x_local = x - obj_coords.x1;

            uint32_t dest_x_idx = (x - layer->buf_area.x1);

            uint16_t * dst_px = (uint16_t*)(dest_row + dest_x_idx * px_size);
            uint16_t * src_px = (uint16_t*)(src_row + src_x_local * px_size);

            *dst_px = *src_px;
        }
    }
}

/**
 * Shared helper to create the test content.
 */
static lv_obj_t * create_test_content(lv_obj_t * parent)
{
    lv_obj_t * temp_cont = lv_obj_create(parent);
    lv_obj_set_size(temp_cont, 200, 200);
    lv_obj_set_style_bg_color(temp_cont, lv_palette_main(LV_PALETTE_BLUE), 0);
    lv_obj_set_style_border_width(temp_cont, 5, 0);
    lv_obj_set_style_border_color(temp_cont, lv_palette_main(LV_PALETTE_RED), 0);
    lv_obj_center(temp_cont);

    /* Add some content */
    lv_obj_t * label = lv_label_create(temp_cont);
    lv_label_set_text(label, "Snapshot\nVerification\nRAM TEST");
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    lv_obj_center(label);

    lv_obj_t * btn = lv_button_create(temp_cont);
    lv_obj_set_size(btn, 100, 40);
    lv_obj_align(btn, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_t * btn_lbl = lv_label_create(btn);
    lv_label_set_text(btn_lbl, "Click Me");
    lv_obj_center(btn_lbl);

    return temp_cont;
}

void run_demo_snapshot_verification(void)
{
#if DEMO_USE_SNAPSHOT_PATH
    /* --- PATH A: Snapshot + Direct Draw --- */

    lv_obj_t * snapshot_target = NULL;

#if DEMO_SNAPSHOT_DOWN_SCALE
    /*
     * STRATEGY: Create a smaller 100x100 wrapper and put the 200x200 content inside it, scaled down.
     * lv_snapshot_take uses the logic size of the target object.
     * So we must snapshot the wrapper (100x100) to get a 100x100 buffer.
     */
    lv_obj_t * wrapper = lv_obj_create(lv_screen_active());
    lv_obj_set_size(wrapper, 100, 100);
    lv_obj_set_style_bg_opa(wrapper, LV_OPA_TRANSP, 0); // Transparent wrapper
    lv_obj_set_style_border_width(wrapper, 0, 0);
    lv_obj_set_style_pad_all(wrapper, 0, 0);

    lv_obj_t * temp_cont = create_test_content(wrapper);

    /* Use Top-Left Pivot strategy to ensure 200x200 maps exactly to 0..100 without clipping */
    lv_obj_set_style_transform_pivot_x(temp_cont, 0, 0);
    lv_obj_set_style_transform_pivot_y(temp_cont, 0, 0);
    lv_obj_set_style_transform_scale(temp_cont, 128, 0); // 50% scale

    /* Align to Top-Left (0,0) */
    lv_obj_align(temp_cont, LV_ALIGN_TOP_LEFT, 0, 0);

    snapshot_target = wrapper;
#else
    /* 1:1 case */
    lv_obj_t * temp_cont = create_test_content(lv_screen_active());
    snapshot_target = temp_cont;
#endif

    /* 2. Update layout */
    lv_obj_update_layout(snapshot_target);

    /* 3. Take snapshot */
    g_snapshot_buf = lv_snapshot_take(snapshot_target, LV_COLOR_FORMAT_RGB565);

    if(!g_snapshot_buf) {
        LV_LOG_USER("Snapshot failed! Trying generic ARGB8888 as backup.");
        g_snapshot_buf = lv_snapshot_take(snapshot_target, LV_COLOR_FORMAT_ARGB8888);
    }

    if(g_snapshot_buf) {
        LV_LOG_USER("Snapshot taken: %dx%d, stride: %d, cf: %d",
            g_snapshot_buf->header.w,
            g_snapshot_buf->header.h,
            g_snapshot_buf->header.stride,
            g_snapshot_buf->header.cf);

        /* 4. Delete the original widget to free RAM (The core point of this test) */
        lv_obj_delete(snapshot_target);

        /* 5. Create display widget */
        lv_obj_t * display_obj = lv_obj_create(lv_screen_active());
        lv_obj_set_size(display_obj, g_snapshot_buf->header.w, g_snapshot_buf->header.h);
        lv_obj_center(display_obj);
        lv_obj_set_style_bg_color(display_obj, lv_color_black(), 0); // Background behind snapshot

        lv_obj_add_event_cb(display_obj, snapshot_draw_event_cb, LV_EVENT_DRAW_MAIN, NULL);

        lv_obj_t * instr = lv_label_create(lv_screen_active());
#if DEMO_SNAPSHOT_DOWN_SCALE
        lv_label_set_text(instr, "Mode: Snapshot(100px) + Direct Draw");
#else
        lv_label_set_text(instr, "Mode: Snapshot(200px) + Direct Draw");
#endif
        lv_obj_set_style_text_color(instr, lv_color_white(), 0);
        lv_obj_align(instr, LV_ALIGN_TOP_MID, 0, 10);
    } else {
        LV_LOG_ERROR("Failed to take snapshot.");
        lv_obj_t * err = lv_label_create(lv_screen_active());
        lv_label_set_text(err, "Snapshot Failed!");
        lv_obj_center(err);
    }

#else
    /* --- PATH B: Standard Widget --- */

    /* 1. Just create and show the content */
    create_test_content(lv_screen_active());

    lv_obj_t * instr = lv_label_create(lv_screen_active());
    lv_label_set_text(instr, "Mode: Standard Widget (Baseline)");
    lv_obj_set_style_text_color(instr, lv_color_white(), 0);
    lv_obj_align(instr, LV_ALIGN_TOP_MID, 0, 10);

#endif
}
