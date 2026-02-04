#!/usr/bin/env python3
import os
import re
import argparse
from pathlib import Path

def parse_c_file(file_path):
    """解析单个C文件，提取图像数组和描述符信息"""
    with open(file_path, 'r', encoding='utf-8') as f:
        content = f.read()

    info = {'path': file_path, 'name': Path(file_path).stem}

    # 1. 查找图像数据数组声明
    array_match = re.search(r'uint8_t\s+(\w+)_map\[\]\s*=', content)
    if array_match:
        info['array_name'] = array_match.group(1)
    else:
        alt_match = re.search(r'uint8_t\s+(\w+)\[\].*=', content)
        if alt_match:
            info['array_name'] = alt_match.group(1)
        else:
            raise ValueError("未找到图像数据数组")

    # 2. 查找 lv_image_dsc_t 结构体变量名
    struct_match = re.search(r'lv_image_dsc_t\s+(\w+)\s*=', content)
    if struct_match:
        info['struct_name'] = struct_match.group(1)
    else:
        info['struct_name'] = info['array_name']

    # 3. 提取图像头信息
    w_match = re.search(r'\.w\s*=\s*(\d+)', content)
    h_match = re.search(r'\.h\s*=\s*(\d+)', content)
    cf_match = re.search(r'\.cf\s*=\s*(LV_COLOR_FORMAT_\w+)', content)
    stride_match = re.search(r'\.stride\s*=\s*(\d+)', content)

    # 4. 提取完整的 flags 信息
    flags_match = re.search(r'\.flags\s*=\s*([^,\n]+)', content)
    if flags_match:
        info['flags'] = flags_match.group(1).strip()
    else:
        info['flags'] = '0'

    if w_match and h_match and cf_match and stride_match:
        info['width'] = int(w_match.group(1))
        info['height'] = int(h_match.group(1))
        info['color_format'] = cf_match.group(1)
        info['stride'] = int(stride_match.group(1))
    else:
        info['width'] = 0
        info['height'] = 0
        info['color_format'] = 'LV_COLOR_FORMAT_UNKNOWN'
        info['stride'] = 0

    # 5. 提取原始数组数据块
    array_start = content.find('{', content.find(info['array_name'] + '_map[] =' if '_map' in info.get('array_name', '') else info['array_name'] + '[] ='))
    array_end = content.rfind('};') + 1

    if array_start != -1 and array_end != -1 and array_end > array_start:
        info['array_data'] = content[array_start:array_end].strip()
    else:
        brace_match = re.search(r'=\s*\{([^}]+(?:\{[^}]*\}[^}]*)*)\}\s*;', content, re.DOTALL)
        if brace_match:
            info['array_data'] = '{' + brace_match.group(1) + '}'
        else:
            raise ValueError("无法提取数组数据")

    # 6. 提取 data_size
    data_size_match = re.search(r'\.data_size\s*=\s*sizeof\((\w+)_map\)', content)
    if not data_size_match:
        data_size_match = re.search(r'\.data_size\s*=\s*(\d+)', content)

    if data_size_match:
        if 'sizeof' in data_size_match.group(0):
            info['data_size_expr'] = f'sizeof({data_size_match.group(1)}_data)'
        else:
            info['data_size'] = int(data_size_match.group(1))
    else:
        hex_values = re.findall(r'0x[0-9a-fA-F]+', info['array_data'])
        info['data_size'] = len(hex_values)

    return info

def generate_files(all_images_info, output_name="C_array"):
    """生成C文件和H文件"""

    # 生成头文件
    output_h = Path(f"{output_name}.h")
    output_c = Path(f"{output_name}.c")

    try:
        # 1. 生成头文件 (.h)
        with open(output_h, 'w', encoding='utf-8') as f:
            f.write(f"""#ifndef LVGL_{output_name.upper()}_H
#define LVGL_{output_name.upper()}_H

#ifdef __cplusplus
extern "C" {{
#endif

#include "lvgl.h"

""")

            # 枚举定义
            f.write(f"/* 图像索引枚举 */\n")
            f.write(f"typedef enum {{\n")
            for idx, info in enumerate(all_images_info):
                enum_name = info['name'].upper()
                enum_name = re.sub(r'[^a-zA-Z0-9_]', '_', enum_name)
                if enum_name[0].isdigit():
                    enum_name = f"IMG_{enum_name}"
                f.write(f"    {enum_name} = {idx},\n")

            f.write(f"""    {output_name.upper()}_IMG_NUM = {len(all_images_info)}
}} {output_name}_img_index_t;\n\n""")

            # 外部声明图像描述符数组（定义在C文件中）
            f.write(f"/* 图像描述符数组（定义在 {output_name}.c 中） */\n")
            f.write(f"extern const lv_image_dsc_t {output_name}_images[{output_name.upper()}_IMG_NUM];\n\n")

            # 快捷访问函数声明
            f.write(f"""/* 获取图像描述符 */
const lv_image_dsc_t * {output_name}_get_img({output_name}_img_index_t idx);

/* 快捷宏 */
#define PIC_ADDR(a) {output_name}_get_img(a)

#ifdef __cplusplus
}}
#endif

#endif /* LVGL_{output_name.upper()}_H */
""")

        # print(f"✓ 生成头文件: {output_h}")

        # 2. 生成C文件 (.c)
        with open(output_c, 'w', encoding='utf-8') as f:
            f.write(f"""#include "{output_name}.h"

""")

            # 写入每个图像的原始数据数组（在C文件中定义）
            for info in all_images_info:
                f.write(f"static const LV_ATTRIBUTE_MEM_ALIGN LV_ATTRIBUTE_LARGE_CONST uint8_t {info['array_name']}_data[] = {info['array_data']};\n\n")

            # 写入合并的图像描述符数组（在C文件中定义）
            f.write(f"const lv_image_dsc_t {output_name}_images[] = {{\n")
            for info in all_images_info:
                f.write(f"    {{\n")
                f.write(f"        .header = {{\n")
                f.write(f"            .magic = LV_IMAGE_HEADER_MAGIC,\n")
                f.write(f"            .cf = {info['color_format']},\n")
                f.write(f"            .flags = {info['flags']},\n")
                f.write(f"            .w = {info['width']},\n")
                f.write(f"            .h = {info['height']},\n")
                f.write(f"            .stride = {info.get('stride', 0)},\n")
                f.write(f"            .reserved_2 = 0,\n")
                f.write(f"        }},\n")
                if 'data_size_expr' in info:
                    f.write(f"        .data_size = {info['data_size_expr']},\n")
                elif 'data_size' in info:
                    f.write(f"        .data_size = {info['data_size']},\n")
                else:
                    f.write(f"        .data_size = sizeof({info['array_name']}_data),\n")
                f.write(f"        .data = {info['array_name']}_data,\n")
                f.write(f"        .reserved = NULL,\n")
                f.write(f"    }},\n\n")
            f.write(f"}};\n\n")

            # 实现快捷访问函数
            f.write(f"""const lv_image_dsc_t * {output_name}_get_img({output_name}_img_index_t idx) {{
    if (idx < {output_name.upper()}_IMG_NUM) {{
        return &{output_name}_images[idx];
    }}
    return NULL;
}}
""")

        print(f"✓ 生成C文件: {output_c}")

    except Exception as e:
        print(f"错误: 生成文件失败 - {str(e)}")
        return False

    return True

def merge_c_files(input_dir, output_name="C_array"):
    """合并目录下的所有C文件，分别生成C和H文件"""
    input_path = Path(input_dir)
    c_files = list(input_path.glob('*.c'))

    if not c_files:
        print("错误: 未找到.c文件")
        return False

    all_images_info = []
    fail_count = 0
    processed_count = 0

    print(f"正在处理目录: {input_dir}")
    print(f"找到 {len(c_files)} 个C文件")

    for cf in c_files:
        try:
            # print(f"  - 解析: {cf.name}", end='')
            info = parse_c_file(cf)
            all_images_info.append(info)
            processed_count += 1
            # print(" ✓")
        except Exception as e:
            fail_count += 1
            print(f" ✗ - {str(e)}")

    if not all_images_info:
        print("错误: 没有成功解析任何文件")
        return False

    # print(f"\n成功解析: {processed_count}/{len(c_files)} 个文件")
    if fail_count > 0:
        print(f"失败解析: {fail_count} 个文件")

    # 生成文件
    success = generate_files(all_images_info, output_name)

    if success:
        print(f"\n文件生成成功:")
        print(f"   头文件: {output_name}.h")
        print(f"   C文件:  {output_name}.c")
        print(f"   包含 {len(all_images_info)} 个图像")

    return success and fail_count == 0

def main():
    parser = argparse.ArgumentParser(
        description='合并LVGL图像C数组文件 - 生成单独的C和H文件'
    )
    parser.add_argument('input_dir', help='包含.c文件的输入目录')
    parser.add_argument('-o', '--output', default='C_array',
                       help='输出文件名前缀（默认: C_array）')

    args = parser.parse_args()

    if not Path(args.input_dir).exists():
        print("错误: 输入目录不存在")
        return

    success = merge_c_files(args.input_dir, args.output)

    if not success:
        print("\n处理失败，请检查错误信息")
        return 1

    print("\n处理完成！")
    return 0

if __name__ == '__main__':
    main()
