import json
import sys
import os

def read_config(config_file="converter_config.json"):
    """读取配置文件"""
    if not os.path.exists(config_file):
        print("Error: Config file not found")
        sys.exit(1)

    try:
        with open(config_file, 'r', encoding='utf-8') as f:
            return json.load(f)
    except Exception as e:
        print(f"Error reading config: {e}")
        sys.exit(1)

def get_config_value(config, section, key):
    """获取配置值"""
    if section in config and key in config[section]:
        return config[section][key]
    return None

if __name__ == "__main__":
    if len(sys.argv) != 3:
        print("Usage: python read_config.py <section> <key>")
        print("Example: python read_config.py bmp_1bit cf")
        sys.exit(1)

    section = sys.argv[1]
    key = sys.argv[2]

    config = read_config()
    value = get_config_value(config, section, key)

    if value is not None:
        print(value)
    else:
        print("")
