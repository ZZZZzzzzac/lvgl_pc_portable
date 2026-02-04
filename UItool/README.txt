.
├── convert_images.bat          # 主批处理脚本
├── converter_config.json       # 转换配置文件
├── read_config.py             # 配置读取脚本
├── LVGLImage_bmp_support.py   # LVGL图片转换脚本
├── merge_lvgl_c_arrays.py     # C数组合并脚本
├── check_bmp_bitdepth.py      # BMP位深度检测脚本
├── lz4_compress           # lz4分块压缩脚本
├── UI/                        # 输入图片文件夹
│   ├── *.bmp
│   └── *.png
├── UI_custom/          # lz4分块图片文件夹
│   ├── *.bmp
│   └── *.png
└── output/                    # 输出文件文件夹
    └── *.c
更改转换配置可打开converter_config.json
合并后的文件C_array.c和C_array.h放入代码中使用

注：图片文件不要重名 首字母不能为数字

