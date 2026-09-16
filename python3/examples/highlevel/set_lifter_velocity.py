"""控制移动版双臂 TRON2 升降台的速度（SDK 文档 3.6.13）。"""

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
    positive_int,
    print_connected,
    validate_connection_arguments,
)


def parse_args():
    parser = argparse.ArgumentParser(description="设置移动版双臂 TRON2 升降台速度")
    parser.add_argument("velocity", type=finite_float, help="速度，单位 mm/s，可正可负")
    parser.add_argument(
        "duration", type=positive_int, help="速度控制持续时间，单位 ms，必须大于 0"
    )
    add_connection_arguments(parser)
    add_confirmation_argument(parser)
    args = parser.parse_args()
    validate_connection_arguments(parser, args)
    return args


def main():
    args = parse_args()
    summary = f"即将控制升降台：velocity={args.velocity} mm/s，duration={args.duration} ms"
    if not confirm_action(summary, args.yes):
        print("已取消，未发送命令")
        return 0
    try:
        with client_from_args(args) as client:
            print_connected(client)
            client.request(
                "request_set_lifter_velocity",
                "response_set_lifter_velocity",
                {"velocity": args.velocity, "duration": args.duration},
            )
            print("升降台速度命令发送成功")
    except (OSError, ProtocolError, websocket.WebSocketException) as error:
        print(f"设置升降台速度失败：{error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())

