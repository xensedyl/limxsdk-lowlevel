"""
通过 WebSocket 调用 MoveH 接口控制机器人头部。

依赖安装：
    pip install websocket-client

运行：
    python3 moveHeadWebSocket.py
"""

import json
import threading
import time
import uuid

import websocket


ROBOT_IP = "10.192.1.2"
ROBOT_PORT = 5000

PITCH_RANGE = (-0.78, 1.04)
YAW_RANGE = (-1.57, 1.57)

accid = None
ws_client = None
robot_info_ready = threading.Event()


def send_moveh(pitch, yaw, duration):
    if not PITCH_RANGE[0] <= pitch <= PITCH_RANGE[1]:
        raise ValueError(f"pitch 必须在 {PITCH_RANGE} 范围内")
    if not YAW_RANGE[0] <= yaw <= YAW_RANGE[1]:
        raise ValueError(f"yaw 必须在 {YAW_RANGE} 范围内")
    if duration <= 0:
        raise ValueError("time 必须大于 0")
    if accid is None:
        raise RuntimeError("尚未获取机器人 ACCID")

    message = {
        "accid": accid,
        "title": "request_moveh",
        "timestamp": int(time.time() * 1000),
        "guid": str(uuid.uuid4()),
        "data": {
            "time": duration,
            "joint": [pitch, yaw],
        },
    }

    print("发送请求：")
    print(json.dumps(message, ensure_ascii=False, indent=2))
    ws_client.send(json.dumps(message, ensure_ascii=False))


def handle_commands():
    print("等待机器人信息...")
    robot_info_ready.wait()
    print(f"已获取机器人 ACCID：{accid}")

    while ws_client and ws_client.sock and ws_client.sock.connected:
        try:
            command = input("输入 moveh 控制头部，输入 exit 退出：\n").strip().lower()
            if command == "exit":
                ws_client.close()
                return
            if command != "moveh":
                print("未知命令")
                continue

            pitch = float(input("请输入 pitch [-0.78, 1.04]："))
            yaw = float(input("请输入 yaw [-1.57, 1.57]："))
            duration = float(input("请输入动作时间（秒）："))
            send_moveh(pitch, yaw, duration)
        except ValueError as error:
            print(f"参数错误：{error}")


def on_open(ws):
    print("WebSocket 已连接")
    threading.Thread(target=handle_commands, daemon=True).start()


def on_message(ws, message):
    global accid

    try:
        response = json.loads(message)
    except json.JSONDecodeError:
        print(f"收到非 JSON 消息：{message}")
        return

    title = response.get("title", "")
    if title == "notify_robot_info":
        received_accid = response.get("accid")
        if received_accid:
            accid = received_accid
            robot_info_ready.set()
        return

    if title == "response_moveh":
        result = response.get("data", {}).get("result")
        print(f"MoveH 响应：{result}")
    else:
        print(f"收到消息：{message}")


def on_error(ws, error):
    print(f"WebSocket 错误：{error}")


def on_close(ws, close_status_code, close_msg):
    print(f"WebSocket 连接已关闭：{close_status_code} {close_msg or ''}".rstrip())


def main():
    global ws_client

    ws_client = websocket.WebSocketApp(
        f"ws://{ROBOT_IP}:{ROBOT_PORT}",
        on_open=on_open,
        on_message=on_message,
        on_error=on_error,
        on_close=on_close,
    )

    print("按 Ctrl+C 可退出")
    try:
        ws_client.run_forever()
    except KeyboardInterrupt:
        ws_client.close()


if __name__ == "__main__":
    main()
