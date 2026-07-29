"""通过 Bridge WebSocket 接收并显示机器人相机视频流。

依赖：
    python3 -m pip install \
        "numpy==1.26.3" "opencv-python==4.11.0.86" websockets

说明：
    limxsdk 4.0.2 要求 numpy<1.26.4，不能安装依赖 NumPy 2 的
    opencv-python 5.x。

退出：
    在视频窗口中按 q 或 Esc。
"""

import argparse
import ssl
import time
from urllib.parse import urlencode

import cv2
import numpy as np
from websockets.sync.client import connect


DEFAULT_HOST = "10.192.1.4"
DEFAULT_TOPIC = "/camera/top/color/image_raw/compressed"


def parse_args():
    parser = argparse.ArgumentParser(description="显示机器人相机视频流")
    parser.add_argument("--host", default=DEFAULT_HOST, help="Bridge 服务 IP")
    parser.add_argument("--topic", default=DEFAULT_TOPIC, help="相机压缩图像话题")
    parser.add_argument("--fps", type=int, default=30, help="最大接收帧率")
    return parser.parse_args()


def create_ssl_context():
    context = ssl.create_default_context()
    # 机器人使用自签名证书，因此测试脚本不校验证书。
    context.check_hostname = False
    context.verify_mode = ssl.CERT_NONE
    return context


def decode_bridge_image(message):
    """解析 BRDG v1 图像消息并返回 OpenCV BGR 图像。"""
    if not isinstance(message, bytes) or len(message) < 14:
        return None
    if message[:4] != b"BRDG" or message[4] != 1:
        return None

    mime_length = message[5]
    image_offset = 6 + mime_length + 8  # mime + uint64 timestamp
    if image_offset >= len(message):
        return None

    encoded_image = np.frombuffer(message[image_offset:], dtype=np.uint8)
    return cv2.imdecode(encoded_image, cv2.IMREAD_COLOR)


def stream_camera(host, topic, fps):
    query = urlencode({"topic": topic, "kind": "image", "max_fps": fps})
    url = f"wss://{host}/bridge/ws?{query}"
    window_name = f"TRON2 Camera - {topic}"
    frame_count = 0
    report_time = time.monotonic()

    print(f"正在连接：{url}")
    print("在视频窗口中按 q 或 Esc 退出")

    try:
        with connect(
            url,
            ssl=create_ssl_context(),
            max_size=None,
        ) as websocket:
            for message in websocket:
                image = decode_bridge_image(message)
                if image is None:
                    continue

                cv2.imshow(window_name, image)
                frame_count += 1

                now = time.monotonic()
                if now - report_time >= 1.0:
                    print(
                        f"图像尺寸：{image.shape[1]}x{image.shape[0]}，"
                        f"接收帧率：{frame_count / (now - report_time):.1f} FPS"
                    )
                    frame_count = 0
                    report_time = now

                key = cv2.waitKey(1) & 0xFF
                if key in (ord("q"), 27):
                    break
    finally:
        cv2.destroyAllWindows()


def main():
    args = parse_args()
    if args.fps <= 0:
        raise ValueError("fps 必须大于 0")

    try:
        stream_camera(args.host, args.topic, args.fps)
    except KeyboardInterrupt:
        pass


if __name__ == "__main__":
    main()
