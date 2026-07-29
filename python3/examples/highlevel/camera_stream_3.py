"""同时接收并显示 TRON2 左、右、顶部三个相机的视频流。

每个相机使用独立同步 WebSocket 连接和接收线程，OpenCV 窗口统一在
主线程中刷新。在任意视频窗口中按 q 或 Esc 退出。
"""

import argparse
import threading
import time
from urllib.parse import urlencode

import cv2
from websockets.sync.client import connect

from camera_stream import create_ssl_context, decode_bridge_image


DEFAULT_HOST = "10.192.1.4"
CAMERA_TOPICS = {
    "Left Camera": "/camera/left/color/image_resized/compressed",
    "Right Camera": "/camera/right/color/image_resized/compressed",
    "Top Camera": "/camera/top/color/image_raw/compressed",
}


def parse_args():
    parser = argparse.ArgumentParser(description="同时显示三个机器人相机视频流")
    parser.add_argument("--host", default=DEFAULT_HOST, help="Bridge 服务 IP")
    parser.add_argument("--fps", type=int, default=31, help="服务端最大放行帧率")
    return parser.parse_args()


class CameraStreams:
    def __init__(self, host, fps):
        self.host = host
        self.fps = fps
        self.stop_event = threading.Event()
        self.lock = threading.Lock()
        self.frames = {}
        self.frame_counts = {name: 0 for name in CAMERA_TOPICS}
        self.connections = {}
        self.threads = []

    def start(self):
        for name, topic in CAMERA_TOPICS.items():
            thread = threading.Thread(
                target=self._receive,
                args=(name, topic),
                daemon=True,
            )
            thread.start()
            self.threads.append(thread)

    def _receive(self, name, topic):
        query = urlencode(
            {"topic": topic, "kind": "image", "max_fps": self.fps}
        )
        url = f"wss://{self.host}/bridge/ws?{query}"
        print(f"{name} 正在连接：{url}")

        try:
            with connect(
                url,
                ssl=create_ssl_context(),
                max_size=None,
            ) as websocket:
                with self.lock:
                    self.connections[name] = websocket

                for message in websocket:
                    if self.stop_event.is_set():
                        break

                    image = decode_bridge_image(message)
                    if image is None:
                        continue

                    with self.lock:
                        self.frames[name] = image
                        self.frame_counts[name] += 1
        except Exception as error:
            if not self.stop_event.is_set():
                print(f"{name} 视频流错误：{error}")
        finally:
            with self.lock:
                self.connections.pop(name, None)

    def get_frames(self):
        with self.lock:
            return self.frames.copy()

    def take_frame_counts(self):
        with self.lock:
            counts = self.frame_counts.copy()
            for name in self.frame_counts:
                self.frame_counts[name] = 0
            return counts

    def stop(self):
        self.stop_event.set()
        with self.lock:
            connections = list(self.connections.values())
        for websocket in connections:
            websocket.close()
        for thread in self.threads:
            thread.join(timeout=2.0)


def main():
    args = parse_args()
    if args.fps <= 0:
        raise ValueError("fps 必须大于 0")

    streams = CameraStreams(args.host, args.fps)
    streams.start()
    report_time = time.monotonic()
    print("在任意视频窗口中按 q 或 Esc 退出")

    try:
        while True:
            for name, image in streams.get_frames().items():
                cv2.imshow(name, image)

            key = cv2.waitKey(1) & 0xFF
            if key in (ord("q"), 27):
                break

            now = time.monotonic()
            elapsed = now - report_time
            if elapsed >= 1.0:
                counts = streams.take_frame_counts()
                rates = "，".join(
                    f"{name}: {counts[name] / elapsed:.1f} FPS"
                    for name in CAMERA_TOPICS
                )
                print(rates)
                report_time = now

            if streams.threads and not any(
                thread.is_alive() for thread in streams.threads
            ):
                print("所有相机连接均已断开")
                break

            time.sleep(0.001)
    except KeyboardInterrupt:
        pass
    finally:
        streams.stop()
        cv2.destroyAllWindows()


if __name__ == "__main__":
    main()
