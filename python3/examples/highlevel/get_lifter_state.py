"""获取移动版双臂 TRON2 的升降台状态（SDK 文档 3.6.11）。"""

import argparse
import json
import math
import sys
import time

import websocket

from mobile_platform_common import (
    ProtocolError,
    add_connection_arguments,
    client_from_args,
    print_connected,
    require_single_number,
    validate_connection_arguments,
)


def parse_args():
    parser = argparse.ArgumentParser(description="读取移动版双臂 TRON2 的升降台状态")
    add_connection_arguments(parser)
    parser.add_argument(
        "--interval", type=float, default=0.0,
        help="每次读取后等待的秒数；默认 0 表示只读取一次",
    )
    args = parser.parse_args()
    validate_connection_arguments(parser, args)
    if not math.isfinite(args.interval) or args.interval < 0:
        parser.error("--interval 必须为大于等于 0 的有限数值")
    return args


def get_lifter_state(client):
    payload, response = client.request(
        "request_lifter_state", "response_lifter_state"
    )
    return {
        "timestamp": response.get("timestamp"),
        "q": require_single_number(payload.get("q"), "q"),
        "v": require_single_number(payload.get("v"), "v"),
    }


def main():
    args = parse_args()
    try:
        with client_from_args(args) as client:
            print_connected(client)
            if args.interval > 0:
                print("按 Ctrl+C 退出连续读取")
            while True:
                print(json.dumps(get_lifter_state(client), ensure_ascii=False, indent=2))
                if args.interval == 0:
                    break
                time.sleep(args.interval)
    except KeyboardInterrupt:
        print("\n已退出")
    except (OSError, ProtocolError, websocket.WebSocketException) as error:
        print(f"读取升降台状态失败：{error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())

