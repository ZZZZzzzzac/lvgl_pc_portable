# 设计文档：零额外缓冲 3D 渲染方案 (Zero Extra Buffer)

## 1. 目标
设计一种内存效率极高的 LVGL 3D 变换方案。该方案通过在渲染阶段直接将 2D 源图像（或组件快照）投影到 3D 平行四边形区域，从而消除对中间帧缓冲区的需求，实现**最小化 RAM 占用**。

## 2. 核心概念：自定义组件与直接绘制

### 2.1 现有方案的内存瓶颈
*   **Snapshot + Canvas (用户原有思路)**: 需要 **2倍内存**。
    1.  源缓冲区 (Source Buffer): 原始 UI 的快照。
    2.  目标画布缓冲区 (Destination Canvas Buffer): 变形后的结果。
*   **ThorVG**: CPU 占用高，且复杂路径下内存不可控。

### 2.2 提议方案 (Custom Widget)
*   **自定义组件 (`lv_3d_face`)**: 我们不把图像画*到*画布上，而是创建一个自定义组件，让组件直接画*在*屏幕上。
*   **直接绘制 (Direct Draw)**: 挂钩标准的 `LV_EVENT_DRAW_MAIN` 事件。
*   **逆向映射 (Inverse Mapping)**: 对于屏幕上组件覆盖的每一个像素 (x, y)，通过数学计算找到它对应在源图中的位置 (u, v)。
*   **双线性插值**: 复用 `src/basic_math.c` 中的算法进行采样。
*   **RAM 占用**: **1倍内存** (仅需源图)。直接写入显示驱动的 flush buffer（或 layer buffer）。

## 3. 架构设计

### 3.1 `lv_3d_face` 对象结构
这是一个自定义的 LVGL 组件 (Widget)，负责渲染 3D 物体的一个面。

```c
typedef struct {
    lv_obj_t obj;                // 继承基础对象
    lv_image_dsc_t * source_img; // 源纹理 (快照或图片)
    lv_obj_t * source_wrapper;   // (可选) 这是一个仅用于包装 source_img 数据给 bilinear_interpolation 使用的"假"画布对象
    lv_point_t points[4];        // 投影到屏幕上的4个顶点坐标 (Top-Left, Top-Right, Bottom-Right, Bottom-Left)
    lv_matrix_t inverse_matrix;  // 预计算的逆矩阵：Screen(x,y) -> Texture(u,v)
} lv_3d_face_t;
```

### 3.2 内存对比 (以 240x240 面, 16-bit 色深为例)

| 组件 | Snapshot + Canvas | **自定义组件 (本方案)** |
| :--- | :--- | :--- |
| **源纹理 (Source)** | ~115 KB | **~115 KB** |
| **目标缓冲 (Dest)** | ~115 KB | **0 KB** (直接上屏) |
| **总 RAM** | **~230 KB** | **~115 KB** |
| **节省** | 0% | **~50%** |

## 4. 实现细节

### 4.1 绘制流程 (伪代码)

该逻辑位于组件的 `LV_EVENT_DRAW_MAIN` 回调中。

**关于 `layer_set_pixel` 的说明**:
LVGL 没有标准的 `layer_set_pixel` API。我们需要直接访问 `lv_layer_t` 内部的 `draw_buf`。
*   在 LVGL v9 中，可以通过 `layer->draw_buf->data` 获取原始指针。
*   我们需要根据 `color_format` (如 RGB565) 手动计算偏移量并写入像素。

```c
static void lv_3d_face_draw_event_cb(lv_event_t * e) {
    lv_obj_t * obj = lv_event_get_target(e);
    lv_layer_t * layer = lv_event_get_layer(e);
    lv_3d_face_t * face = (lv_3d_face_t *)obj;

    // 0. 获取图层缓冲区的原始指针 (Direct Buffer Access)
    // 注意：这需要包含 LVGL 内部头文件或使用特定的 draw_buf API
    uint8_t * buf_start = layer->draw_buf->data;
    uint32_t stride = layer->draw_buf->header.stride;
    int32_t buf_w = layer->draw_buf->header.w;
    int32_t buf_h = layer->draw_buf->header.h;

    // 1. 计算 4 个顶点的包围盒 (Bounding Box)
    int32_t min_x = MIN(p0.x, p1.x, ...);
    int32_t max_x = MAX(p0.x, p1.x, ...);
    int32_t min_y = MIN(p0.y, p1.y, ...);
    int32_t max_y = MAX(p0.y, p1.y, ...);

    // 与当前重绘区域求交集 (Clip)
    int32_t clip_x1 = MAX(min_x, layer->_clip_area.x1);
    int32_t clip_y1 = MAX(min_y, layer->_clip_area.y1);
    int32_t clip_x2 = MIN(max_x, layer->_clip_area.x2);
    int32_t clip_y2 = MIN(max_y, layer->_clip_area.y2);

    // 2. 像素遍历 (Scanline)
    for (int y = clip_y1; y <= clip_y2; y++) {
        // 优化：计算当前行的行首指针
        uint8_t * row_ptr = buf_start + y * stride;

        for (int x = clip_x1; x <= clip_x2; x++) {

            // 3. 几何测试：(x,y) 是否在投影后的四边形内？
            // 复用 src/transform_3d.c 中的 point_in_quad
            if (!point_in_quad((Coord2D){x,y}, face->points)) {
                continue;
            }

            // 4. 逆向映射 (Inverse Mapping)
            // 将屏幕坐标 (x,y) 映射回 源纹理坐标 (u,v)
            float u, v;
            // 注意：这里需要实现一个辅助函数，应用 face->inverse_matrix
            apply_matrix_transform(&face->inverse_matrix, x, y, &u, &v);

            // 5. 双线性插值采样 (Bilinear Interpolation)
            // 复用 src/basic_math.c 中的逻辑
            // 注意：bilinear_interpolation 需要 lv_obj_t* (Canvas)。
            // 我们需要创建一个临时的 "Wrapper Canvas" 或者修改该函数接受 lv_image_dsc_t。
            // 假设我们传入的是包装好的 face->source_wrapper:
            lv_color32_t c = bilinear_interpolation(face->source_wrapper, u, v);

            // 6. 写入像素到屏幕缓冲
            // 假设是 RGB565 格式
            uint16_t * px_ptr = (uint16_t*)(row_ptr + x * 2);
            *px_ptr = lv_color_to16(c); // 需要根据实际颜色格式转换
        }
    }
}
```

### 4.2 适配 `bilinear_interpolation`

`src/basic_math.c` 中的 `bilinear_interpolation` 函数签名如下：
`lv_color32_t bilinear_interpolation(lv_obj_t* obj, float src_x, float src_y)`

为了复用此代码且不创建额外的像素缓冲区：
1.  **方法 A (推荐)**: 修改 `bilinear_interpolation`，使其接受 `lv_image_dsc_t *` 而不是 `lv_obj_t *`。
2.  **方法 B (兼容)**: 在 `lv_3d_face` 初始化时，创建一个 "虚假" 的 `lv_canvas` 对象。
    *   使用 `lv_canvas_create(NULL)`。
    *   使用 `lv_canvas_set_buffer(canvas, snapshot_dsc->data, w, h, format)`。
    *   这样 `bilinear_interpolation` 就可以直接通过 `lv_canvas_get_px` 访问快照数据，而**不会复制内存**。

### 4.3 矩阵计算 (复用逻辑)

继续使用 `src/transform_3d.c` 中的 `calculate_inverse_transform_matrix`。
*   **输入 Src**: 纹理的四个角 `(0,0), (w,0), (w,h), (0,h)`。
*   **输入 Dst**: 屏幕上投影后的四个点 `points[4]`。
*   **输出**: `inverse_matrix`。

### 4.4 进阶绘制接口 (Refinement: Drawing Context)

为了支持分区域绘制、非矩形绘制以及多次绘制同一对象的需求，我们将绘制逻辑从事件回调中解耦。

**核心思想**:
创建一个通用的“绘制上下文” (`lv_draw_direct_ctx_t`)，它封装了直接缓冲区访问的所有必要信息。上层绘制函数只需接受此上下文，无需关心底层的 LVGL 图层细节。

**数据结构**:
```c
typedef struct {
    uint8_t * buf_start;     // 缓冲区起始地址
    uint32_t stride;         // 行跨度 (字节)
    lv_color_format_t cf;    // 颜色格式
    lv_area_t buf_area;      // 缓冲区在屏幕坐标系中的绝对区域
    lv_area_t clip_area;     // 当前裁剪区域 (已与 obj_area 求交集)
} lv_draw_direct_ctx_t;
```

**工作流程**:
1.  在 `LV_EVENT_DRAW_MAIN` 中，初始化 `lv_draw_direct_ctx_t`。
2.  调用特定的绘制函数，如 `draw_triangle(ctx, ...)` 或 `draw_polygon(ctx, ...)`。
3.  每个绘制函数内部：
    *   计算需要绘制的 Bounding Box。
    *   将其与 `ctx->clip_area` 进一步求交集。
    *   遍历像素，使用 `ctx->buf_start` 和 `ctx->buf_area` 计算偏移量并写入。

**优势**:
*   **灵活性**: 可以在一个 `DRAW` 事件中多次调用不同的绘制函数，绘制复杂的组合图形。
*   **可重用性**: 底层的像素写入逻辑被封装，可供不同的 Shape 复用。
*   **清晰性**: `main.c` 或组件代码只需关注“画什么”，而不用处理“怎么写内存”。

## 5. 开发计划

1.  **准备阶段**:
    *   修改 `src/basic_math.c`: 确保插值函数可用（可能需要调整入参类型或使用 Wrapper Canvas 技巧）。
    *   暴露 `src/transform_3d.c` 中的 `point_in_quad` 和矩阵计算函数。

2.  **实现 `lv_3d_face`**:
    *   创建 `lv_3d_face.c` 和 `lv_3d_face.h`。
    *   实现组件的基本生命周期（Create, Delete）。
    *   实现核心的 `DRAW` 事件处理，包含直接缓冲区写入逻辑。

3.  **集成**:
    *   在 `main.c` 中，用 `lv_3d_face` 替换原本的 Canvas 或 Image 对象。
    *   设置 Snapshot 作为 `lv_3d_face` 的源。
    *   在拖拽回调中，更新 `lv_3d_face` 的顶点数据，而不是设置 `transform` 矩阵。

## 6. 分步实施计划 (Step-by-Step Implementation Plan)

为了降低风险并便于调试，我们将实施分为三个独立的演示 (Demo) 阶段。

### 阶段 1: Demo 1 - 自定义直接绘制验证 (Direct Draw Verification)
**目标**: 验证通过 `LV_EVENT_DRAW_MAIN` 直接操作 `layer->draw_buf` 像素的可行性。

**实现步骤**:
1.  创建 `src/demo_direct_draw.c`。
2.  创建一个简单的 LVGL 组件（例如继承自 `lv_obj`）。
3.  添加 `LV_EVENT_DRAW_MAIN` 回调。
4.  在回调中：
    *   获取 `lv_layer_t * layer`。
    *   获取 `layer->draw_buf` 的数据指针。
    *   **不使用** `lv_draw_rect`，而是通过指针操作，将组件区域内的像素涂成**红色**。
    *   处理 `stride` (行跨度) 和 `color_format` (RGB565)。
5.  在 `main.c` 中运行此 Demo。
**预期结果**: 屏幕上出现一个红色矩形，且不依赖标准的绘制函数。

### 阶段 2: Demo 2 - 逆向映射逻辑验证 (Math Verification)
**目标**: 验证“屏幕坐标 -> 纹理坐标”逆向映射矩阵计算的正确性，而不涉及复杂的 UI 渲染。

**实现步骤**:
1.  创建 `src/demo_inverse_matrix.c`。
2.  定义一个源图像缓冲区 (Source Buffer)，填充简单的棋盘格图案。
3.  定义一个 3D 透视变形的目标四边形坐标。
4.  编写一个纯 C 函数 `render_transform_to_buffer(src_buf, dst_buf, quad_points)`：
    *   计算逆矩阵。
    *   遍历目标缓冲区的每一个像素 (x, y)。
    *   使用 `point_in_quad` 判断是否在四边形内。
    *   应用逆矩阵计算 (u, v)。
    *   从源缓冲区采样颜色写入目标缓冲区。
5.  将结果缓冲区保存为图片，或通过 LVGL 简单的 Image 对象显示出来进行肉眼验证。
**预期结果**: 生成的图像中，棋盘格图案正确地适配到了目标四边形中，无明显扭曲错误。

### 阶段 3: Demo 3 - 完整集成 (Full Integration)
**目标**: 结合阶段 1 和 阶段 2，实现最终的 `lv_3d_face` 组件。

**实现步骤**:
1.  创建 `src/lv_3d_face.c` 和 `.h`。
2.  移植阶段 2 的数学逻辑到阶段 1 的绘制回调中。
3.  集成 `lv_snapshot_take`：
    *   创建一个复杂的 UI 容器（包含按钮、标签等）。
    *   对其进行快照，获取 `lv_image_dsc_t`。
    *   将此快照设置为 `lv_3d_face` 的源。
4.  在 `main.c` 中实现交互：
    *   鼠标拖拽更新 3D 旋转角度。
    *   实时更新 `lv_3d_face` 的顶点坐标。
    *   组件触发重绘，实时显示 3D 变换后的 UI。
**预期结果**: 一个低内存占用的、可交互旋转的 3D UI 面。
