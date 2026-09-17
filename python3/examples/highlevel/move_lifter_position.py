"""控制移动版双臂 TRON2 升降台的绝对位置（SDK 文档 3.6.12）。"""

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
    nonnegative_int,
    print_connected,
    validate_connection_arguments,
)


def parse_args():
    parser = argparse.ArgumentParser(description="设置移动版双臂 TRON2 升降台绝对位置")
    parser.add_argument("position", type=finite_float, help="目标位置，单位 mm")
    parser.add_argument(
        "duration", type=nonnegative_int,
        help="到达时间，单位 ms；允许 0，表示最快速度到达",
    )
    add_connection_arguments(parser)
    add_confirmation_argument(parser)
    args = parser.parse_args()
    validate_connection_arguments(parser, args)
    return args


def main():
    args = parse_args()
    summary = f"即将设置升降台：position={args.position} mm，duration={args.duration} ms"
    if not confirm_action(summary, args.yes):
        print("已取消，未发送命令")
        return 0
    try:
        with client_from_args(args) as client:
            print_connected(client)
            client.request(
                "request_set_lifter_position",
                "response_set_lifter_position",
                {"position": args.position, "duration": args.duration},
            )
            print("升降台绝对位置命令发送成功")
    except (OSError, ProtocolError, websocket.WebSocketException) as error:
        print(f"设置升降台绝对位置失败：{error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
