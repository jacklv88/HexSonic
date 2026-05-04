import os
import sys
import argparse
from datetime import datetime

# ================= 核心配置区域 =================

# 1. 想要扫描的目标文件后缀 (ESP/C++工程常用)
TARGET_EXTENSIONS = (
    '.c', '.h', '.cpp', '.hpp',  # C/C++ 源码
    'CMakeLists.txt',            # 构建脚本
    'sdkconfig',                 # ESP 配置 (通常没有后缀)
    'sdkconfig.defaults',
    '.conf', '.prj', '.in',       # 其他常见配置  
    '.png','.jpg',
)

# 2. 这里填入您不想看到的文件名或目录名
# ESP工程通常包含 build 目录和 managed_components，建议屏蔽以减小体积
CUSTOM_BLOCK_LIST = [
    'build',                # 编译输出目录 (非常重要，否则会扫描大量生成的中间文件)
    'managed_components',   # IDF 组件管理器下载的库 (如果只想看自己的代码，建议屏蔽)
    '.git',                 # git 目录
    '.vscode',              # vscode 配置
    '.idea',                # jetbrains 配置
    'dist',                 # python 构建产物
    'sdkconfig',
    # 'main.c',
    # 'driver_lcd_touch.c',
    # 'driver_lcd_touch.h',
    # 'driver_io.h',
    # 'driver_io.c',
    'CST816D.c',
    'CST816D.h',
    'esp_lcd_spd2010.c',
    'esp_lcd_spd2010.h',
    'image.jpg',
    'image.png',
    # 'lvgl_img_jpeg_png_test.',
    # 'lvgl_img_jpeg_png_test.h',

]

# 3. 自动生成的垃圾代码后缀或特定文件 (无需修改，可按需添加)
AUTO_GENERATED_SUFFIXES = (
    '.o', '.obj', '.bin', '.hex', '.map', '.elf'
)

# ==============================================

def should_ignore(file_path, project_root):
    """判断文件或目录是否应该被忽略"""
    filename = os.path.basename(file_path)
    rel_path = os.path.relpath(file_path, project_root)
    path_parts = rel_path.split(os.sep)

    # 1. 检查是否在屏蔽列表中 (目录或文件)
    for pattern in CUSTOM_BLOCK_LIST:
        # 如果配置项包含 '/' 或 '\', 则视为路径匹配
        if '/' in pattern or '\\' in pattern:
            norm_pattern = pattern.replace('/', os.sep).replace('\\', os.sep)
            clean_pattern = norm_pattern.strip(os.sep)
            if clean_pattern in path_parts:
                return True
        else:
            # 简单名称匹配 (匹配文件夹名 或 文件名)
            if pattern in path_parts:
                return True
    
    # 2. 如果是文件，检查是否是目标后缀
    if os.path.isfile(file_path):
        # 特殊处理：如果文件名完全匹配某些无后缀文件 (如 sdkconfig)
        if filename in TARGET_EXTENSIONS:
            return False
            
        # 检查后缀
        if not filename.endswith(TARGET_EXTENSIONS):
            return True
            
        # 检查自动生成的二进制后缀 (作为双重保险)
        if filename.endswith(AUTO_GENERATED_SUFFIXES):
            return True

    return False

def read_file_content(file_path):
    """读取文件内容"""
    try:
        file_size = os.path.getsize(file_path)
        
        # 限制单文件大小 (10MB)
        if file_size > 10 * 1024 * 1024:
            return f"⚠️ [文件过大 - 已跳过] ({file_size/1024/1024:.1f}MB)"
        
        with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
            content = f.read()
        
        # 简洁的头部
        header = f"// ------------------------------------------\n"
        header += f"// 📄 {os.path.basename(file_path)}\n"
        header += f"// ------------------------------------------\n"
        
        return header + content
    except Exception as e:
        return f"// ❌ [读取错误] {e}"

def get_project_structure(root_path):
    """生成目录树（包含被过滤的项目，但会进行标记）"""
    structure = []
    project_name = os.path.basename(os.path.abspath(root_path))
    structure.append(f"📂 {project_name}/ (Root)")
    
    for root, dirs, files in os.walk(root_path):
        # 排除掉被屏蔽的目录，避免 walk 深入进去浪费时间 (特别是 build 目录)
        # 这一步修改 dirs 列表会影响 os.walk 的后续遍历
        dirs[:] = [d for d in dirs if not should_ignore(os.path.join(root, d), root_path)]
        
        dirs.sort()
        files.sort()
        
        rel_path = os.path.relpath(root, root_path)
        
        if rel_path == '.':
            level = 0
        else:
            level = rel_path.count(os.sep) + 1
            
        indent = "│   " * level
        
        # --- 显示文件夹 ---
        if level > 0:
            folder_name = os.path.basename(root)
            structure.append(f"{indent[:-4]}├── 📁 {folder_name}/")
        
        # --- 显示文件 ---
        sub_indent = "│   " * (level + 1)
        for i, file in enumerate(files):
            full_path = os.path.join(root, file)
            
            # 这里只显示我们需要关注的代码文件，杂乱的文件直接不显示在树里，保持清爽
            if file.endswith(TARGET_EXTENSIONS) or file in TARGET_EXTENSIONS:
                if should_ignore(full_path, root_path):
                     structure.append(f"{sub_indent}📄 {file} [🚫 已过滤]")
                else:
                    structure.append(f"{sub_indent}📄 {file}")
            
    return "\n".join(structure)

def process_project(project_path):
    if not os.path.exists(project_path):
        return f"❌ 找不到项目目录: {project_path}"

    output = []
    
    # 头部信息
    output.append("ESP-IDF / C++ 代码导出")
    output.append("=" * 50)
    output.append(f"时间: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
    output.append(f"屏蔽规则: {CUSTOM_BLOCK_LIST}")
    output.append("说明: 仅导出 .c, .h, .cpp, CMakeLists.txt 等源码文件。")
    output.append("\n")

    output.append(f"项目说明:")
    output.append("说明: esp32s3 的 16MB flash 和 2MB psram的模块. 基于 ESP-idf 5.5.1 版本的工程.")
    output.append("\n")
    output.append("LVGL 的版本是 9.5.0  esp_lvgl_port 的版本是 2.7.2")
    output.append("\n")


    # 目录树
    output.append("项目结构:")
    output.append("-" * 30)
    output.append(get_project_structure(project_path))
    output.append("\n" + "=" * 50 + "\n")

    # 扫描并读取文件
    files_to_process = []
    for root, dirs, files in os.walk(project_path):
        # 优化：同样在遍历时跳过屏蔽目录
        dirs[:] = [d for d in dirs if not should_ignore(os.path.join(root, d), project_path)]
        
        for file in files:
            full_path = os.path.join(root, file)
            
            # 1. 必须是目标后缀
            is_target = file.endswith(TARGET_EXTENSIONS) or file in TARGET_EXTENSIONS
            
            # 2. 且不应该被忽略
            if is_target and not should_ignore(full_path, project_path):
                rel_path = os.path.relpath(full_path, project_path)
                files_to_process.append((rel_path, full_path))
    
    files_to_process.sort()
    
    if not files_to_process:
        return "⚠️ 没有找到符合条件的文件 (请检查路径或屏蔽规则)"

    # 输出文件内容
    for rel_path, full_path in files_to_process:
        output.append(f"PATH: {rel_path}")
        output.append(read_file_content(full_path))
        output.append("\n\n")

    return "\n".join(output)

def main():
    parser = argparse.ArgumentParser()
    # 默认路径改为当前目录，或者您可以设为您的常用 ESP 工程路径
    parser.add_argument('path', nargs='?', default='./HexSonic', help='ESP工程路径')
    parser.add_argument('-o', '--output', default='HexSonic_source.txt', help='输出文件名')
    args = parser.parse_args()
    
    abs_path = os.path.abspath(args.path)
    print(f"🔍 正在扫描: {abs_path}")
    print(f"🎯 目标类型: {TARGET_EXTENSIONS}")
    
    try:
        content = process_project(abs_path)
        with open(args.output, 'w', encoding='utf-8') as f:
            f.write(content)
        
        size_kb = os.path.getsize(args.output) / 1024
        print(f"✅ 导出成功: {args.output} ({size_kb:.1f} KB)")
        
    except Exception as e:
        print(f"❌ 错误: {e}")
        import traceback
        traceback.print_exc()

if __name__ == "__main__":
    main()