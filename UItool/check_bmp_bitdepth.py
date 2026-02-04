#!/usr/bin/env python3
import struct
import sys

def get_bmp_bitdepth(filename):
    """获取BMP文件的位深度"""
    try:
        with open(filename, 'rb') as f:
            # 读取BMP文件头
            f.seek(0)
            signature = f.read(2)
            if signature != b'BM':
                return -1  # 不是有效的BMP文件

            # 跳过文件大小等字段
            f.seek(28)
            bit_depth = struct.unpack('<H', f.read(2))[0]
            return bit_depth
    except Exception as e:
        return -1

if __name__ == "__main__":
    if len(sys.argv) < 2:

        sys.exit(1)

    filename = sys.argv[1]
    bitdepth = get_bmp_bitdepth(filename)
    print(bitdepth)
