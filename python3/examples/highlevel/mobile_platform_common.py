"""移动版双臂 TRON2 WebSocket 示例的公共协议代码。"""

import argparse
from contextlib import AbstractContextManager
import json
import math
import time
import uuid

import websocket


DEFAULT_ROBOT_IP = "10.192.1.2"
DEFAULT_ROBOT_PORT = 5000


class ProtocolError(RuntimeError):
    """机器人响应不符合协议或请求执行失败。"""


def finite_float(value):
    """供 argparse 使用的有限浮点数类型。"""
    try:
        number = float(value)
    except ValueError as error:
        raise argparse.ArgumentTypeError("必须是数值") from error
    if not math.isfinite(number):
        raise argparse.ArgumentTypeError("必须是有限数值")
    return number


def nonnegative_int(value):
    try:
        number = int(value)
    except ValueError as error:
        raise argparse.ArgumentTypeError("必须是整数") from error
    if number < 0:
        raise argparse.ArgumentTypeError("必须大于等于 0")
    return number


def positive_int(value):
    number = nonnegative_int(value)
    if number == 0:
        raise argparse.ArgumentTypeError("必须大于 0")
    return number


def add_connection_arguments(parser):
    parser.add_argument("--host", default=DEFAULT_ROBOT_IP, help="机器人 IP")
    parser.add_argument(
        "--port", type=int, default=DEFAULT_ROBOT_PORT, help="WebSocket 端口"
    )
    parser.add_argument("--accid", help="机器人实际序列号；默认从机器人消息自动获取")
    parser.add_argument(
        "--timeout", type=finite_float, default=5.0, help="各阶段超时秒数，默认 5"
    )


def validate_connection_arguments(parser, args):
    if not 1 <= args.port <= 65535:
        parser.error("--port 必须在 1～65535 之间")
    if args.timeout <= 0:
        parser.error("--timeout 必须大于 0")
    if args.accid is not None:
        args.accid = args.accid.strip()
        if not args.accid:
            parser.error("--accid 不能为空")


def add_confirmation_argument(parser):
    parser.add_argument("--yes", action="store_true", help="跳过发送前的交互确认")


def confirm_action(summary, assume_yes=False):
    """显示待发送参数，并要求明确输入 yes。"""
    print(summary)
    if assume_yes:
        return True
    try:
        answer = input("确认机器人周围安全后输入 yes 发送，其他输入取消：")
    except (EOFError, KeyboardInterrupt):
        print()
        return False
    return answer.strip().lower() == "yes"


def require_number(value, field):
    if type(value) not in (int, float) or not math.isfinite(value):
        raise ProtocolError(f"响应字段 {field} 必须是有限数值，实际为 {value!r}")
    return value


def require_single_number(values, field):
    if not isinstance(values, list) or len(values) != 1:
        raise ProtocolError(f"响应字段 {field} 必须是单元素数组，实际为 {values!r}")
    return require_number(values[0], f"{field}[0]")


class MobilePlatformClient(AbstractContextManager):
    """移动版双臂接口的同步请求客户端。"""

    def __init__(self, host, port, timeout=5.0, accid=None):
        self.host = host
        self.port = port
        self.timeout = timeout
        self.accid = accid
        self.ws = None

    @property
    def url(self):
        return f"ws://{self.host}:{self.port}"

    def connect(self):
        self.ws = websocket.create_connection(self.url, timeout=self.timeout)
        try:
            if self.accid is None:
                self.accid = self._wait_for_accid()
        except Exception:
            self.close()
            raise
        return self

    def close(self):
        if self.ws is not None:
            self.ws.close()
            self.ws = None

    def __enter__(self):
        return self.connect()

    def __exit__(self, exc_type, exc_value, traceback):
        self.close()
        return False

    def _receive_json(self, deadline, description):
        """使用固定截止时间接收，忽略非 JSON 和非对象消息。"""
        while True:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                raise TimeoutError(f"{description}超时")
            self.ws.settimeout(remaining)
            try:
                message = self.ws.recv()
            except websocket.WebSocketTimeoutException as error:
                raise TimeoutError(f"{description}超时") from error
            if message in ("", b""):
                raise ConnectionError("机器人已关闭 WebSocket 连接")
            try:
                response = json.loads(message)
            except (json.JSONDecodeError, UnicodeDecodeError):
                continue
            if isinstance(response, dict):
                return response

    def _wait_for_accid(self):
        deadline = time.monotonic() + self.timeout
        while True:
            response = self._receive_json(
                deadline,
                "等待 notify_robot_info（也可用 --accid 指定实际序列号）",
            )
            if response.get("title") != "notify_robot_info":
                continue
            accid = response.get("accid")
            if not accid and isinstance(response.get("data"), dict):
                accid = response["data"].get("accid")
            if isinstance(accid, str) and accid.strip():
                return accid.strip()

    def request(self, request_title, response_title, data=None):
        """发送请求，返回成功响应中的 data 对象和外层响应。"""
        if self.ws is None:
            raise RuntimeError("WebSocket 尚未连接")
        guid = str(uuid.uuid4())
        message = {
            "accid": self.accid,
            "title": request_title,
            "guid": guid,
            "timestamp": time.time_ns() // 1_000_000,
            "data": {} if data is None else data,
        }
        self.ws.settimeout(self.timeout)
        self.ws.send(json.dumps(message))

        deadline = time.monotonic() + self.timeout
        while True:
            response = self._receive_json(deadline, f"等待 {response_title}")
            if (
                response.get("title") != response_title
                or response.get("guid") != guid
                or response.get("accid") != self.accid
            ):
                continue
            payload = response.get("data")
            if not isinstance(payload, dict):
                raise ProtocolError(
                    "响应 data 不是对象：" + json.dumps(response, ensure_ascii=False)
                )
            if payload.get("result") != "success":
                result = payload.get("result", "missing_result")
                if result == "fail_no_data":
                    return None, response
                raise ProtocolError(f"{request_title} 请求失败：{result}")
            return payload, response


def client_from_args(args):
    return MobilePlatformClient(
        host=args.host,
        port=args.port,
        timeout=args.timeout,
        accid=args.accid,
    )


def print_connected(client):
    print(f"已连接：{client.url}")
    print(f"机器人 ACCID：{client.accid}")
