# ahv_decoder.py
import os
import struct
import argparse
from typing import List, Tuple
from pathlib import Path


class AHVDecoder:
    def __init__(self):
        self.header_magic = bytes([0x2e, 0x61, 0x68, 0x76])  # ".ahv"

    def decode_ahv_file(self, file_path: str) -> dict:
        """
        解码AHV文件

        Args:
            file_path: AHV文件路径

        Returns:
            包含文件信息和帧数据的字典
        """
        with open(file_path, 'rb') as f:
            data = f.read()

        return self.decode_ahv_data(data)

    def decode_ahv_data(self, data: bytes) -> dict:
        """
        解码AHV数据

        Args:
            data: AHV二进制数据

        Returns:
            包含文件信息和帧数据的字典
        """
        if len(data) < 69:  # 头部最小长度
            raise ValueError("Invalid AHV file: file too small")

        # 解析文件头
        magic = data[0:4]
        if magic != self.header_magic:
            raise ValueError(f"Invalid magic number: {magic.hex()}")

        version = data[4:8]
        display_mode = data[8]
        frame_count = int.from_bytes(data[9:13], byteorder='big')
        frame_width = int.from_bytes(data[13:15], byteorder='big')
        frame_height = int.from_bytes(data[15:17], byteorder='big')
        frame_interval = int.from_bytes(data[17:19], byteorder='big')
        reserved = data[19:69]

        # 解析帧数据
        frames_data = []
        offset = 69  # 头部结束位置

        for i in range(frame_count):
            if offset + 4 > len(data):
                raise ValueError(f"Unexpected end of file while reading frame {i} length")

            # 读取帧长度
            frame_length = int.from_bytes(data[offset:offset + 4], byteorder='big')
            offset += 4

            if offset + frame_length > len(data):
                raise ValueError(f"Unexpected end of file while reading frame {i} data")

            # 读取帧数据
            frame_data = data[offset:offset + frame_length]
            offset += frame_length

            frames_data.append(frame_data)

        return {
            'magic': magic,
            'version': version,
            'display_mode': display_mode,
            'frame_count': frame_count,
            'frame_width': frame_width,
            'frame_height': frame_height,
            'frame_interval': frame_interval,
            'reserved': reserved,
            'frames_data': frames_data,
            'total_size': len(data)
        }

    def save_frames_to_directory(self, decoded_data: dict, output_dir: str, prefix: str = "frame"):
        """
        将解码的帧数据保存为JPEG文件

        Args:
            decoded_data: 解码后的数据字典
            output_dir: 输出目录
            prefix: 文件名前缀
        """
        # 创建输出目录
        Path(output_dir).mkdir(parents=True, exist_ok=True)

        frames_data = decoded_data['frames_data']

        for i, frame_data in enumerate(frames_data):
            # 生成文件名
            filename = f"{prefix}_{i:04d}.jpg"
            file_path = os.path.join(output_dir, filename)

            # 保存JPEG文件
            with open(file_path, 'wb') as f:
                f.write(frame_data)

            print(f"Saved frame {i} to {file_path}")

    def print_file_info(self, decoded_data: dict):
        """
        打印文件信息

        Args:
            decoded_data: 解码后的数据字典
        """
        print("=== AHV File Information ===")
        print(f"Magic: {decoded_data['magic'].hex()}")
        print(f"Version: {decoded_data['version'].hex()}")
        print(
            f"Display Mode: {decoded_data['display_mode']} ({'Same Screen' if decoded_data['display_mode'] == 0 else 'Different Screen'})")
        print(f"Frame Count: {decoded_data['frame_count']}")
        print(f"Frame Size: {decoded_data['frame_width']}x{decoded_data['frame_height']}")
        print(f"Frame Interval: {decoded_data['frame_interval']}ms")
        print(f"Reserved Bytes: {len(decoded_data['reserved'])}")
        print(f"Total File Size: {decoded_data['total_size']} bytes")
        print(f"Total Frames Data: {len(decoded_data['frames_data'])} frames")

        # 计算帧数据总大小
        total_frame_size = sum(len(frame) for frame in decoded_data['frames_data'])
        print(f"Frames Data Total Size: {total_frame_size} bytes")
        print("=" * 30)


def verify_frame_integrity(decoded_data: dict):
    """
    验证帧数据的完整性

    Args:
        decoded_data: 解码后的数据字典
    """
    print("\n=== Frame Integrity Check ===")
    frames_data = decoded_data['frames_data']

    for i, frame_data in enumerate(frames_data):
        # 检查JPEG文件头
        if len(frame_data) >= 2:
            jpeg_header = frame_data[:2]
            if jpeg_header == b'\xff\xd8':  # JPEG文件头
                status = "✓ Valid JPEG"
            else:
                status = "✗ Invalid JPEG header"
        else:
            status = "✗ Frame too small"

        print(f"Frame {i:3d}: Size={len(frame_data):6d} bytes - {status}")


def main():
    parser = argparse.ArgumentParser(description="AHV 文件解码工具 - 提取二进制包中的图片帧")

    # 必需参数
    parser.add_argument("-i", "--input", required=True, help="输入 AHV 文件路径 (必需)")

    # 可选参数
    parser.add_argument("-o", "--output", default="extracted_frames",
                        help="输出图片保存的目录 (默认: extracted_frames)")
    parser.add_argument("-p", "--prefix", default="frame", help="输出图片的文件名前缀 (默认: frame)")

    # 标志位开关
    parser.add_argument("--info-only", action="store_true", help="仅打印文件信息，不执行导出帧的操作")
    parser.add_argument("--verify", action="store_true", help="执行完成后进行帧完整性(JPEG 头)检查")

    args = parser.parse_args()

    decoder = AHVDecoder()

    try:
        if not os.path.exists(args.input):
            print(f"错误: 找不到输入文件 '{args.input}'")
            return

        print(f"正在读取并解码: {args.input} ...\n")
        decoded_data = decoder.decode_ahv_file(args.input)

        # 打印文件信息
        decoder.print_file_info(decoded_data)

        # 根据标志位决定是否导出图片
        if not args.info_only:
            print(f"\n正在提取帧到目录: {args.output}")
            decoder.save_frames_to_directory(decoded_data, args.output, args.prefix)
            print(f"\n成功提取了 {len(decoded_data['frames_data'])} 帧。")

        # 验证完整性
        if args.verify:
            verify_frame_integrity(decoded_data)

    except Exception as e:
        print(f"\n解码过程中发生错误: {e}")


if __name__ == "__main__":
    main()