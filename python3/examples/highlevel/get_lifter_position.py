"""查询移动版双臂 TRON2 升降台当前位置（SDK 文档 3.6.14）。"""

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
    require_number,
    validate_connection_arguments,
)


def parse_args():
    parser = argparse.ArgumentParser(description="查询移动版双臂 TRON2 升降台当前位置")
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


def get_lifter_position(client):
    payload, response = client.request(
        "request_get_lifter_position", "response_get_lifter_position"
    )
    print("response_get_lifter_position 完整响应体：")
    print(json.dumps(response, ensure_ascii=False, indent=2))
    if not isinstance(payload, dict):
        raise ProtocolError(
            "response_get_lifter_position 的 data 不是对象："
            + json.dumps(response, ensure_ascii=False)
        )
    # 实机 data.timestamp 可能是浮点数，与外层时间戳的单位/时钟来源
    # 不一定相同。保留原始数值，不强制整数，也不推测单位进行换算。
    data_timestamp = require_number(payload.get("timestamp"), "data.timestamp")
    return {
        "response_timestamp": response.get("timestamp"),
        "position": require_number(payload.get("position"), "position"),
        "q": require_number(payload.get("q"), "q"),
        "q_per_mm": require_number(payload.get("q_per_mm"), "q_per_mm"),
        "timestamp": data_timestamp,
    }


def main():
    args = parse_args()
    try:
        with client_from_args(args) as client:
            print_connected(client)
            if args.interval > 0:
                print("按 Ctrl+C 退出连续读取")
            while True:
                state = get_lifter_position(client)
                print(json.dumps(state, ensure_ascii=False, indent=2))
                if args.interval == 0:
                    break
                time.sleep(args.interval)
    except KeyboardInterrupt:
        print("\n已退出")
    except (OSError, ProtocolError, websocket.WebSocketException) as error:
        print(f"查询升降台当前位置失败：{error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
