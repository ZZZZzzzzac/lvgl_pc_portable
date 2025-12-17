#include "lvgl/lvgl.h"
#include <cstdlib>
#include <new>

extern "C" {
void * lv_malloc(size_t size);
void lv_free(void * data);
}

// 简单的嵌入式内存分配器
void* operator new(size_t size) {
    void* ptr = lv_malloc(size);
    if (!ptr) {
        // 处理内存分配失败
        LV_LOG_ERROR("lv_malloc fail %d", size);
        while(1);
    }
    return ptr;
}

void operator delete(void* ptr) noexcept {
    lv_free(ptr);
}

void operator delete(void* ptr, size_t size) noexcept {
    LV_UNUSED(size);
    lv_free(ptr);
}

void* operator new[](size_t size) {
    return operator new(size);
}

void operator delete[](void* ptr) noexcept {
    operator delete(ptr);
}

void operator delete[](void* ptr, size_t size) noexcept {
    LV_UNUSED(size);
    operator delete(ptr);
}
