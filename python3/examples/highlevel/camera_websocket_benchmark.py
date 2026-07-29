"""同时测试三个相机 Bridge WebSocket 的纯接收性能。

测试期间只接收并统计 BRDG 图像消息，不进行 JPEG 解码、OpenCV 显示或
磁盘写入。这样可以判断三路视频流通过 Bridge/WSS 到达客户端时能否接近
30 Hz。

默认测试 20 秒。指定 ``--save-dir`` 时，测试期间会把压缩消息保存在内存
中，测试结束后再把原始 JPEG/PNG 数据写入磁盘；长时间测试可能占用较多
内存。
"""

import argparse
import ssl
import threading
import time
from dataclasses import dataclass, field
from pathlib import Path
from typing import List, Optional
from urllib.parse import urlencode

from websockets.sync.client import connect


DEFAULT_HOST = "10.192.1.4"
CAMERA_TOPICS = {
    "left": "/camera/left/color/image_resized/compressed",
    "right": "/camera/right/color/image_resized/compressed",
    "top": "/camera/top/color/image_raw/compressed",
}


def parse_args():
    parser = argparse.ArgumentParser(
        description="同时测试三路相机 Bridge WebSocket 的纯接收帧率"
    )
    parser.add_argument("--host", default=DEFAULT_HOST, help="Bridge 服务 IP")
    parser.add_argument(
        "--fps",
        type=int,
        default=100,
        help="请求的服务端最大放行帧率；默认 100，避免限制 30 Hz 视频源",
    )
    parser.add_argument(
        "--duration",
        type=float,
        default=20.0,
        help="正式统计时长，单位秒（默认 20）",
    )
    parser.add_argument(
        "--warmup",
        type=float,
        default=3.0,
        help="三路连接完成后的预热时长，单位秒（默认 3）",
    )
    parser.add_argument(
        "--report-interval",
        type=float,
        default=1.0,
        help="实时统计打印间隔，单位秒（默认 1）",
    )
    parser.add_argument(
        "--connect-timeout",
        type=float,
        default=15.0,
        help="等待三路 WebSocket 全部连接的超时时间（默认 15）",
    )
    parser.add_argument(
        "--save-dir",
        type=Path,
        help="测试结束后保存全部压缩帧；测试期间帧会保存在内存中",
    )
    return parser.parse_args()


def create_ssl_context():
    context = ssl.create_default_context()
    # 机器人使用自签名证书，测试脚本不校验证书。
    context.check_hostname = False
    context.verify_mode = ssl.CERT_NONE
    return context


def bridge_image_offset(message):
    """返回压缩图像偏移量，不复制 payload；消息无效时返回 None。"""
    if not isinstance(message, bytes) or len(message) < 14:
        return None
    if message[:4] != b"BRDG" or message[4] != 1:
        return None

    mime_length = message[5]
    image_offset = 6 + mime_length + 8  # MIME + uint64 timestamp
    if image_offset >= len(message):
        return None

    return image_offset


def extract_bridge_image(message):
    """测试结束后提取 MIME 和压缩图像数据。"""
    image_offset = bridge_image_offset(message)
    if image_offset is None:
        return None

    mime_length = message[5]
    mime = message[6 : 6 + mime_length].decode("ascii", errors="replace")
    return mime, message[image_offset:]


@dataclass
class CameraStats:
    connected: bool = False
    frames: int = 0
    byte_count: int = 0
    non_image_messages: int = 0
    error: Optional[str] = None
    saved_messages: List[bytes] = field(default_factory=list)


class WebSocketBenchmark:
    def __init__(self, args):
        self.args = args
        self.lock = threading.Lock()
        self.stop_event = threading.Event()
        self.measure_start = None
        self.measure_end = None
        self.stats = {name: CameraStats() for name in CAMERA_TOPICS}
        self.connections = {}
        self.threads = []

    def start_receivers(self):
        for name, topic in CAMERA_TOPICS.items():
            thread = threading.Thread(
                target=self._receive,
                args=(name, topic),
                name=f"camera-ws-{name}",
                daemon=True,
            )
            thread.start()
            self.threads.append(thread)

    def _receive(self, name, topic):
        query = urlencode(
            {"topic": topic, "kind": "image", "max_fps": self.args.fps}
        )
        url = f"wss://{self.args.host}/bridge/ws?{query}"
        print(f"[{name}] 正在连接：{url}")

        try:
            with connect(
                url,
                ssl=create_ssl_context(),
                # 图像 payload 已是 JPEG/PNG，再做 WebSocket deflate 只会
                # 增加 CPU 开销，通常无法继续压缩。
                compression=None,
                max_size=None,
                open_timeout=self.args.connect_timeout,
                close_timeout=2,
            ) as websocket:
                with self.lock:
                    self.connections[name] = websocket
                    self.stats[name].connected = True
                print(f"[{name}] 已连接")

                while not self.stop_event.is_set():
                    try:
                        message = websocket.recv(timeout=1.0)
                    except TimeoutError:
                        continue

                    now = time.monotonic()
                    if bridge_image_offset(message) is None:
                        with self.lock:
                            self.stats[name].non_image_messages += 1
                        continue

                    with self.lock:
                        camera = self.stats[name]
                        start = self.measure_start
                        end = self.measure_end
                        if start is not None and start <= now < end:
                            camera.frames += 1
                            camera.byte_count += len(message)
                            if self.args.save_dir is not None:
                                # bytes 是不可变对象，保存引用不会再次复制帧数据。
                                camera.saved_messages.append(message)

                    if self.measure_end is not None and now >= self.measure_end:
                        break
        except Exception as error:
            if not self.stop_event.is_set():
                with self.lock:
                    self.stats[name].error = f"{type(error).__name__}: {error}"
        finally:
            with self.lock:
                self.connections.pop(name, None)

    def wait_until_connected(self):
        deadline = time.monotonic() + self.args.connect_timeout
        while time.monotonic() < deadline:
            with self.lock:
                errors = {
                    name: camera.error
                    for name, camera in self.stats.items()
                    if camera.error is not None
                }
                all_connected = all(
                    camera.connected for camera in self.stats.values()
                )

            if errors:
                details = "；".join(
                    f"{name}: {error}" for name, error in errors.items()
                )
                raise RuntimeError(f"WebSocket 连接失败：{details}")
            if all_connected:
                return
            time.sleep(0.05)

        with self.lock:
            missing = [
                name
                for name, camera in self.stats.items()
                if not camera.connected
            ]
        raise TimeoutError(f"等待 WebSocket 连接超时：{', '.join(missing)}")

    def begin_measurement(self):
        now = time.monotonic()
        self.measure_start = now + self.args.warmup
        self.measure_end = self.measure_start + self.args.duration

    def snapshot(self):
        with self.lock:
            return {
                name: (
                    camera.frames,
                    camera.byte_count,
                    camera.non_image_messages,
                    camera.error,
                )
                for name, camera in self.stats.items()
            }

    def stop(self):
        self.stop_event.set()
        with self.lock:
            connections = list(self.connections.values())
        for websocket in connections:
            try:
                websocket.close()
            except Exception:
                pass
        for thread in self.threads:
            thread.join(timeout=3.0)

    def save_compressed_frames(self):
        if self.args.save_dir is None:
            return

        output_root = self.args.save_dir.expanduser().resolve()
        output_root.mkdir(parents=True, exist_ok=True)

        print(f"\n接收测试已经结束，开始保存压缩帧到：{output_root}")
        with self.lock:
            saved = {
                name: list(camera.saved_messages)
                for name, camera in self.stats.items()
            }

        for name, messages in saved.items():
            camera_dir = output_root / name
            camera_dir.mkdir(parents=True, exist_ok=True)
            written = 0

            for index, message in enumerate(messages):
                parsed = extract_bridge_image(message)
                if parsed is None:
                    continue
                mime, encoded_image = parsed
                extension = {
                    "image/jpeg": ".jpg",
                    "image/jpg": ".jpg",
                    "image/png": ".png",
                }.get(mime.lower(), ".bin")
                path = camera_dir / f"{index:06d}{extension}"
                path.write_bytes(encoded_image)
                written += 1

            print(f"[{name}] 已保存 {written} 帧")


def format_interval(name, delta_frames, delta_bytes, elapsed):
    fps = delta_frames / elapsed if elapsed > 0 else 0.0
    mbps = delta_bytes * 8 / elapsed / 1_000_000 if elapsed > 0 else 0.0
    average_kib = (
        delta_bytes / delta_frames / 1024 if delta_frames > 0 else 0.0
    )
    return (
        f"{name}: {fps:5.1f} FPS, {mbps:5.1f} Mbit/s, "
        f"{average_kib:6.1f} KiB/帧"
    )


def validate_args(args):
    if args.fps <= 0:
        raise ValueError("fps 必须大于 0")
    if args.duration <= 0:
        raise ValueError("duration 必须大于 0")
    if args.warmup < 0:
        raise ValueError("warmup 不能小于 0")
    if args.report_interval <= 0:
        raise ValueError("report-interval 必须大于 0")
    if args.connect_timeout <= 0:
        raise ValueError("connect-timeout 必须大于 0")


def main():
    args = parse_args()
    validate_args(args)

    if args.save_dir is not None:
        estimated_mb = args.duration * 9.0
        print(
            "注意：--save-dir 会在测试期间缓存全部压缩帧，"
            f"按当前三路码率预计约占用 {estimated_mb:.0f} MB 内存。"
        )

    benchmark = WebSocketBenchmark(args)
    interrupted = False
    actual_end = None

    try:
        benchmark.start_receivers()
        benchmark.wait_until_connected()
        benchmark.begin_measurement()

        if args.warmup > 0:
            print(f"\n三路均已连接，预热 {args.warmup:.1f} 秒……")
        while True:
            remaining = benchmark.measure_start - time.monotonic()
            if remaining <= 0:
                break
            time.sleep(min(0.1, remaining))

        print(
            f"开始同时接收三路视频，统计 {args.duration:.1f} 秒；"
            "测试阶段不解码、不显示、不写磁盘。"
        )

        # 计数器在正式窗口开始前保持为零。这里从零开始，避免遗漏主线程
        # 刚结束预热到取得第一次 snapshot 之间收到的少量帧。
        previous = {
            name: (0, 0, 0, None) for name in CAMERA_TOPICS
        }
        previous_time = benchmark.measure_start

        while True:
            now = time.monotonic()
            if now >= benchmark.measure_end:
                actual_end = benchmark.measure_end
                break

            time.sleep(min(args.report_interval, benchmark.measure_end - now))
            now = min(time.monotonic(), benchmark.measure_end)
            current = benchmark.snapshot()
            elapsed = now - previous_time

            lines = []
            for name in CAMERA_TOPICS:
                delta_frames = current[name][0] - previous[name][0]
                delta_bytes = current[name][1] - previous[name][1]
                lines.append(
                    format_interval(name, delta_frames, delta_bytes, elapsed)
                )
            total_elapsed = now - benchmark.measure_start
            print(f"[{total_elapsed:5.1f}s] " + " | ".join(lines))

            previous = current
            previous_time = now
    except KeyboardInterrupt:
        interrupted = True
        actual_end = time.monotonic()
        print("\n收到 Ctrl+C，提前结束测试。")
    finally:
        if actual_end is None:
            actual_end = time.monotonic()
        benchmark.stop()

    if benchmark.measure_start is None:
        raise RuntimeError("测试尚未开始")

    measured_duration = max(
        0.0,
        min(actual_end, benchmark.measure_end) - benchmark.measure_start,
    )
    final = benchmark.snapshot()

    print("\n========== 最终结果 ==========")
    print(f"有效统计时长：{measured_duration:.3f} 秒")
    for name in CAMERA_TOPICS:
        frames, byte_count, non_image_messages, error = final[name]
        fps = frames / measured_duration if measured_duration > 0 else 0.0
        mbps = (
            byte_count * 8 / measured_duration / 1_000_000
            if measured_duration > 0
            else 0.0
        )
        average_kib = byte_count / frames / 1024 if frames else 0.0
        percent_of_30 = fps / 30.0 * 100
        print(
            f"[{name}] {frames} 帧，{fps:.2f} FPS "
            f"({percent_of_30:.1f}% of 30 Hz)，{mbps:.2f} Mbit/s，"
            f"平均 {average_kib:.1f} KiB/帧，非图像消息 {non_image_messages}"
        )
        if error is not None:
            print(f"[{name}] 接收错误：{error}")

    benchmark.save_compressed_frames()

    if interrupted:
        print("测试被提前终止，最终结果只覆盖实际统计时长。")


if __name__ == "__main__":
    main()
