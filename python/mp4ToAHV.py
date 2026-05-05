# video_packer.py
import cv2
import struct
import argparse
from io import BytesIO
from PIL import Image
import os


class VideoFramePacker:
    """
    Video -> 自定义二进制包打包类

    打包格式（按题目）：
    - header 4 bytes: 0x2e 0x61 0x68 0x76
    - version 4 bytes: 0x00 0x00 0x00 0x00
    - display mode 1 byte: 0x00 (同屏) or 0x01 (异屏)
    - frame count 4 bytes (uint32)
    - frame width 2 bytes (uint16)
    - frame height 2 bytes (uint16)
    - frame interval 2 bytes (uint16)   <-- 单位 ms，若超 65535 可能需改
    - reserved 50 bytes (填 0)
    - 接着每帧：4 bytes (uint32 length) + frame bytes
    注意：所有整数采用 big-endian（'>I', '>H'）
    """

    MAGIC = b'\x2e\x61\x68\x76'
    VERSION = b'\x00\x00\x00\x00'

    def __init__(self, frame_width=240, frame_height=240,
                 frame_interval_ms=None, duration_ms=3000,
                 jpeg_quality=85):
        # 默认每秒 15 帧（约 66 ms）
        if frame_interval_ms is None:
            frame_interval_ms = int(1000 / 15)
        self.frame_width = int(frame_width)
        self.frame_height = int(frame_height)
        self.frame_interval_ms = int(frame_interval_ms)
        self.duration_ms = int(duration_ms)
        self.jpeg_quality = int(jpeg_quality)

    def _open_cap(self, path):
        cap = cv2.VideoCapture(path)
        if not cap.isOpened():
            raise RuntimeError(f"无法打开视频: {path}")
        return cap

    def _read_frame_at(self, cap, t_ms):
        """
        使用 cv2.CAP_PROP_POS_MSEC 跳到指定毫秒并读取一帧。
        返回 BGR numpy array 或 None。
        """
        cap.set(cv2.CAP_PROP_POS_MSEC, float(t_ms))
        ret, frame = cap.read()
        if not ret:
            return None
        return frame

    def _crop_and_resize(self, frame, crop_box=None):
        """
        frame: BGR numpy array
        crop_box: (x, y, w, h) — 如果为 None，则按目标长宽比居中裁剪
        返回 PIL.Image RGB
        """
        h, w = frame.shape[:2]
        if crop_box:
            x, y, cw, ch = crop_box
            # 限制到边界
            x = max(0, min(int(x), w - 1))
            y = max(0, min(int(y), h - 1))
            cw = max(1, min(int(cw), w - x))
            ch = max(1, min(int(ch), h - y))
            crop = frame[y:y + ch, x:x + cw]
        else:
            target_ratio = self.frame_width / self.frame_height
            src_ratio = w / h
            if src_ratio > target_ratio:
                # 源更宽 -> 按高度裁宽
                new_h = h
                new_w = int(h * target_ratio)
            else:
                # 源更高或等 -> 按宽裁高
                new_w = w
                new_h = int(w / target_ratio)
            start_x = (w - new_w) // 2
            start_y = (h - new_h) // 2
            crop = frame[start_y:start_y + new_h, start_x:start_x + new_w]

        resized = cv2.resize(crop, (self.frame_width, self.frame_height),
                             interpolation=cv2.INTER_LINEAR)
        # BGR -> RGB
        img = Image.fromarray(cv2.cvtColor(resized, cv2.COLOR_BGR2RGB))
        return img

    def _frame_to_jpeg_bytes(self, pil_img):
        """
        保存为 baseline JPEG（非 progressive），返回 bytes
        """
        buf = BytesIO()
        # baseline: progressive=False, optimize=False
        pil_img.save(buf, format='JPEG', quality=self.jpeg_quality,
                     optimize=False, progressive=False)
        return buf.getvalue()

    def extract_frames_from_video(self, video_path, crop_box=None):
        cap = cv2.VideoCapture(video_path)
        if not cap.isOpened():
            raise RuntimeError(f"无法打开视频: {video_path}")

        fps = cap.get(cv2.CAP_PROP_FPS)
        if fps <= 0:
            fps = 30.0

        total_frames = int(cap.get(cv2.CAP_PROP_FRAME_COUNT))
        frame_interval = int(round((self.frame_interval_ms / 1000.0) * fps))
        max_frames = int(round((self.duration_ms / 1000.0) * fps))

        frames = []
        frame_index = 0
        saved_count = 0

        while cap.isOpened():
            ret, frame = cap.read()
            if not ret:
                break
            if frame_index > max_frames:
                break

            if frame_index % frame_interval == 0:
                img = self._crop_and_resize(frame, crop_box=crop_box)
                jpeg_bytes = self._frame_to_jpeg_bytes(img)
                frames.append(jpeg_bytes)
                saved_count += 1

            frame_index += 1

        cap.release()
        print(f"共提取 {saved_count} 帧 (来自 {video_path})")
        return frames

    def _write_header(self, fobj, mode, frame_count):
        """
        写文件头（magic, version, mode, frame_count, width, height, interval, reserved(50)）
        mode: 0 or 1
        """
        fobj.write(self.MAGIC)
        fobj.write(self.VERSION)
        # display mode 1 byte
        fobj.write(struct.pack('>B', 1 if mode else 0))
        # frame count 4 bytes
        fobj.write(struct.pack('>I', frame_count))
        # frame width uint16, frame height uint16
        fobj.write(struct.pack('>H', self.frame_width))
        fobj.write(struct.pack('>H', self.frame_height))
        # frame interval uint16 (ms) — 若超过 65535，会截断，高于请自行调整格式
        interval_to_write = self.frame_interval_ms if self.frame_interval_ms <= 0xFFFF else 0xFFFF
        fobj.write(struct.pack('>H', int(interval_to_write)))
        # reserved 50 bytes zero
        fobj.write(b'\x00' * 50)

    def _write_frames(self, fobj, frames_list):
        """
        frames_list: list of bytes
        每帧前写 4 字节长度（uint32 big-endian），然后写具体数据
        """
        for data in frames_list:
            fobj.write(struct.pack('>I', len(data)))
            fobj.write(data)

    def pack_single_video(self, video_path, output_path, crop_box=None):
        """
        同屏刷新（mode=0）：一个视频
        """
        frames = self.extract_frames_from_video(video_path, crop_box=crop_box)
        with open(output_path, 'wb') as f:
            self._write_header(f, mode=0, frame_count=len(frames))
            self._write_frames(f, frames)
        return len(frames)

    def pack_dual_video(self, video_path_a, video_path_b, output_path,
                        crop_box_a=None, crop_box_b=None):
        """
        异屏刷新（mode=1）：两个视频交替保存 A,B,A,B...
        如果某一侧没有帧则跳过该帧位置（不插入空帧）。
        """
        frames_a = self.extract_frames_from_video(video_path_a, crop_box=crop_box_a)
        frames_b = self.extract_frames_from_video(video_path_b, crop_box=crop_box_b)
        merged = []
        max_len = max(len(frames_a), len(frames_b))
        for i in range(max_len):
            if i < len(frames_a):
                merged.append(frames_a[i])
            if i < len(frames_b):
                merged.append(frames_b[i])
        with open(output_path, 'wb') as f:
            self._write_header(f, mode=1, frame_count=len(merged))
            self._write_frames(f, merged)
        return len(merged)


def main():
    parser = argparse.ArgumentParser(description="视频打包工具 - 将MP4转换为自定义二进制包")

    # 输入与输出文件参数
    parser.add_argument("-i", "--input", required=True, help="输入视频路径 (必需)")
    parser.add_argument("-i2", "--input2", default=None, help="第二个输入视频路径 (如果提供，将采用异屏模式打包)")
    parser.add_argument("-o", "--output", required=True, help="输出文件路径 (例如: output.ahv)")

    # 视频处理可选参数
    parser.add_argument("--width", type=int, default=240, help="输出帧宽度 (默认: 240)")
    parser.add_argument("--height", type=int, default=240, help="输出帧高度 (默认: 240)")
    parser.add_argument("--fps", type=int, default=15, help="提取帧率 (默认: 15)")
    parser.add_argument("--duration", type=int, default=5000, help="截取最大时长，单位毫秒 (默认: 5000)")
    parser.add_argument("--quality", type=int, default=65, help="JPEG 压缩质量 1-100 (默认: 65)")

    args = parser.parse_args()

    # 初始化打包器
    packer = VideoFramePacker(
        frame_width=args.width,
        frame_height=args.height,
        frame_interval_ms=int(1000 / args.fps),
        duration_ms=args.duration,
        jpeg_quality=args.quality
    )

    try:
        # 判断是双视频(异屏)还是单视频(同屏)
        if args.input2:
            print(f"正在处理双视频(异屏模式) -> {args.output} ...")
            m = packer.pack_dual_video(args.input, args.input2, args.output)
            print(f"双视频处理完成，合并帧数: {m}")
        else:
            print(f"正在处理单视频(同屏模式) -> {args.output} ...")
            n = packer.pack_single_video(args.input, args.output)
            print(f"单视频处理完成，帧数: {n}")

    except Exception as e:
        print("发生错误:", e)


if __name__ == "__main__":
    main()