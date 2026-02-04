#ifndef _LV_LZ4_DECODER_H_
#define _LV_LZ4_DECODER_H_

#include "lvgl.h"
#ifdef __cplusplus
extern "C" {
#endif

// 只暴露初始化函数，其他的一律不放这里
void lv_lz4_decoder_init(void);

void lv_lz4_image_set_custom_draw(lv_obj_t * img);
#ifdef __cplusplus
}
#endif

#endif // _LV_LZ4_DECODER_H_
