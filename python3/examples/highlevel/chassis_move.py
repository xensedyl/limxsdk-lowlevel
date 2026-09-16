"""控制移动版双臂 TRON2 的底盘运动（SDK 文档 3.6.17）。"""

import argparse
import sys

import websocket

from mobile_platform_common import (
    ProtocolError,
    add_confirmation_argument,
    add_connection_arguments,
    client_from_args,
    confirm_action,
    finite_float,
    print_connected,
    validate_connection_arguments,
)


def normalized_value(value):
    number = finite_float(value)
    if not -1.0 <= number <= 1.0:
        raise argparse.ArgumentTypeError("必须在 -1 到 1 之间")
    return number


def parse_args():
    parser = argparse.ArgumentParser(description="控制移动版双臂 TRON2 底盘运动")
    parser.add_argument("x", type=normalized_value, help="x 方向速度值，范围 [-1, 1]")
    parser.add_argument("y", type=normalized_value, help="y 方向速度值，范围 [-1, 1]")
    parser.add_argument("yaw", type=normalized_value, help="转弯角速度值，范围 [-1, 1]")
    add_connection_arguments(parser)
    add_confirmation_argument(parser)
    args = parser.parse_args()
    validate_connection_arguments(parser, args)
    return args


def main():
    args = parse_args()
    summary = f"即将发送底盘运动命令：x={args.x}，y={args.y}，yaw={args.yaw}"
    if not confirm_action(summary, args.yes):
        print("已取消，未发送命令")
        return 0
    try:
        with client_from_args(args) as client:
            print_connected(client)
            client.request(
                "request_chassis_move",
                "response_chassis_move",
                {"x": args.x, "y": args.y, "yaw": args.yaw},
            )
            print("底盘运动命令发送成功")
    except (OSError, ProtocolError, websocket.WebSocketException) as error:
        print(f"控制底盘运动失败：{error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())

