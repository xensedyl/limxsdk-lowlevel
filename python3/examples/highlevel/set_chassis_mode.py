"""设置移动版双臂 TRON2 的底盘运动模式（SDK 文档 3.6.16）。"""

import argparse
import sys

import websocket

from mobile_platform_common import (
    ProtocolError,
    add_confirmation_argument,
    add_connection_arguments,
    client_from_args,
    confirm_action,
    print_connected,
    validate_connection_arguments,
)


MOVE_MODES = ("ackerman", "parallel", "park", "spinning", "emergency_stop")


def parse_args():
    parser = argparse.ArgumentParser(description="设置移动版双臂 TRON2 底盘运动模式")
    parser.add_argument("move_mode", choices=MOVE_MODES, help="目标底盘运动模式")
    add_connection_arguments(parser)
    add_confirmation_argument(parser)
    args = parser.parse_args()
    validate_connection_arguments(parser, args)
    return args


def main():
    args = parse_args()
    if not confirm_action(f"即将设置底盘模式：move_mode={args.move_mode}", args.yes):
        print("已取消，未发送命令")
        return 0
    try:
        with client_from_args(args) as client:
            print_connected(client)
            client.request(
                "request_set_chassis_mode",
                "response_set_chassis_mode",
                {"move_mode": args.move_mode},
            )
            print(f"底盘模式设置成功：{args.move_mode}")
    except (OSError, ProtocolError, websocket.WebSocketException) as error:
        print(f"设置底盘模式失败：{error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())

