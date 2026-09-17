"""获取移动版双臂 TRON2 的底盘状态（只读）。

依赖：python3 -m pip install websocket-client
单次：python3 get_chassis_state.py
连续：python3 get_chassis_state.py --interval 0.5

协议：SDK 开发指南 3.6.15，request_chassis_state / response_chassis_state。
返回顺序：linear_velocity、angular_velocity、steering_angle。
该节文档未标注单位，脚本直接输出机器人返回的原始数值。
"""

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
    parser = argparse.ArgumentParser(description="读取移动版双臂 TRON2 的底盘状态")
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


def get_chassis_state(client):
    """发送一次底盘状态请求，并解析三个状态值。"""
    payload, response = client.request(
        "request_chassis_state", "response_chassis_state"
    )
    if not isinstance(payload, dict):
        raise ProtocolError(
            "response_chassis_state 的 data 不是对象："
            + json.dumps(response, ensure_ascii=False)
        )
    values = payload.get("data")
    if not isinstance(values, list) or len(values) != 3:
        raise ProtocolError(f"底盘状态 data 必须包含 3 个数值，实际为 {values!r}")
    return {
        "timestamp": response.get("timestamp"),
        "linear_velocity": require_number(values[0], "data[0]"),
        "angular_velocity": require_number(values[1], "data[1]"),
        "steering_angle": require_number(values[2], "data[2]"),
    }


def main():
    args = parse_args()
    try:
        with client_from_args(args) as client:
            print_connected(client)
            print("底盘状态：linear_velocity=线速度，angular_velocity=角速度，steering_angle=转向角")
            if args.interval > 0:
                print("按 Ctrl+C 退出连续读取")
            while True:
                state = get_chassis_state(client)
                print(json.dumps(state, ensure_ascii=False, indent=2), flush=True)
                if args.interval == 0:
                    break
                time.sleep(args.interval)
    except KeyboardInterrupt:
        print("\n已退出")
    except (OSError, ProtocolError, websocket.WebSocketException) as error:
        print(f"读取底盘状态失败：{error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
