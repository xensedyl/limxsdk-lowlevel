"""键盘按住运动、松开停止（移动版双臂 TRON2，SDK 文档 3.6.17）。

运行：python3 move_chassis.py --speed 0.2 --turn-speed 0.2
依赖：python3 -m pip install websocket-client pygame
需要桌面显示环境；点击控制窗口后使用 W/S、A/D、Q/E。
"""

import argparse
from dataclasses import dataclass
import json
import sys
import threading
import time
import uuid

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


ZERO = (0.0, 0.0, 0.0)
MOTION_KEYS = frozenset(("w", "s", "a", "d", "q", "e", "up", "down", "left", "right"))


def speed_value(value):
    number = finite_float(value)
    if not 0.0 < number <= 1.0:
        raise argparse.ArgumentTypeError("必须大于 0 且不超过 1")
    return number


def parse_args():
    parser = argparse.ArgumentParser(description="TRON2 底盘键盘控制：按住运动，松开停止")
    parser.add_argument("--speed", type=speed_value, default=0.5, help="x/y 速度值，默认 0.5")
    parser.add_argument("--turn-speed", type=speed_value, default=0.5, help="yaw 值，默认 0.5")
    parser.add_argument("--rate", type=finite_float, default=20.0, help="指令刷新频率 Hz，默认 20")
    parser.add_argument(
        "--response-timeout", type=finite_float, default=0.5,
        help="运动指令响应超时秒数，默认 0.5；超时尝试停止并退出",
    )
    parser.add_argument(
        "--mode", choices=("ackerman", "parallel", "park", "spinning", "emergency_stop"),
        help="连接后设置底盘模式；默认保持当前模式",
    )
    add_connection_arguments(parser)
    parser.add_argument(
        "--debug", action="store_true",
        help="打印请求/响应、按键事件，每秒统计持续发送并只读查询底盘状态",
    )
    add_confirmation_argument(parser)
    args = parser.parse_args()
    validate_connection_arguments(parser, args)
    if not 1 <= args.rate <= 100:
        parser.error("--rate 必须在 1～100 Hz 之间")
    if args.response_timeout <= 0:
        parser.error("--response-timeout 必须大于 0")
    return args


class KeyboardControl:
    """失焦或按空格后，必须先松开方向键，才允许再次运动。"""

    def __init__(self, speed, turn_speed):
        self.speed = speed
        self.turn_speed = turn_speed
        self.armed = False

    def command(self, keys, focused):
        if not focused or "space" in keys:
            self.armed = False
            return ZERO
        if not self.armed:
            if not MOTION_KEYS.intersection(keys):
                self.armed = True
            return ZERO

        forward = bool({"w", "up"}.intersection(keys))
        backward = bool({"s", "down"}.intersection(keys))
        left = bool({"a", "left"}.intersection(keys))
        right = bool({"d", "right"}.intersection(keys))
        return (
            self.speed * (forward - backward),
            self.speed * (("q" in keys) - ("e" in keys)),
            self.turn_speed * (left - right),
        )


@dataclass
class PendingCommand:
    sent_at: float
    command: tuple
    report_response: bool


class MotionStream:
    """主线程发送最新指令；后台接收 ACK，松键不用等待之前的 ACK。"""

    def __init__(self, client, response_timeout, debug=False):
        self.ws = client.ws
        self.accid = client.accid
        self.response_timeout = response_timeout
        self.pending = {}
        self.error = None
        self.condition = threading.Condition()
        self.closed = threading.Event()
        self.receiver = None
        self.stop_guid = None
        self.stop_acked = False
        self.debug = debug
        self.last_command = None
        self.nonzero_sent = 0
        self.nonzero_acked = 0
        self.last_ack = "waiting"
        # 诊断请求和运动 ACK 分开关联；查询无反馈不能冒充零速度。
        self.state_pending = None
        self.state_response = None
        self.state_received_at = None
        self.state_query_status = "尚未查询"
        self.robot_info = None
        self.robot_info_received_at = None

    def start(self):
        # 限制 send/recv 的单次阻塞；不要在发送线程中等待业务响应。
        self.ws.settimeout(0.1)
        self.receiver = threading.Thread(target=self._receive, daemon=True)
        self.receiver.start()

    def _fail(self, message):
        with self.condition:
            if self.error is None:
                self.error = message
            self.condition.notify_all()

    def _receive(self):
        while not self.closed.is_set():
            try:
                message = self.ws.recv()
                if message in ("", b""):
                    self._fail("机器人关闭了 WebSocket 连接")
                    return
                try:
                    response = json.loads(message)
                except (json.JSONDecodeError, UnicodeDecodeError):
                    continue
                if not isinstance(response, dict):
                    continue
                title = response.get("title")
                response_accid = response.get("accid")
                if title == "notify_robot_info" and not response_accid:
                    payload = response.get("data")
                    if isinstance(payload, dict):
                        response_accid = payload.get("accid")
                if response_accid != self.accid:
                    continue
                if self.debug and title == "notify_robot_info":
                    with self.condition:
                        self.robot_info = response
                        self.robot_info_received_at = time.monotonic()
                        self.condition.notify_all()
                    continue
                if self.debug and title == "response_chassis_state":
                    with self.condition:
                        if self.state_pending is not None and response.get("guid") == self.state_pending[0]:
                            self.state_response = response
                            self.state_received_at = time.monotonic()
                            self.state_pending = None
                            self.state_query_status = "已收到响应（内容见原始响应体）"
                            self.condition.notify_all()
                    continue
                if title != "response_chassis_move":
                    continue
                guid = response.get("guid")
                if not isinstance(guid, str):
                    continue
                with self.condition:
                    if guid not in self.pending:
                        continue
                    pending = self.pending.pop(guid)
                    payload = response.get("data")
                    if not isinstance(payload, dict) or payload.get("result") != "success":
                        self._fail("底盘返回失败响应：" + json.dumps(response, ensure_ascii=False))
                    else:
                        if pending.command != ZERO:
                            self.nonzero_acked += 1
                        x, y, yaw = pending.command
                        self.last_ack = f"success x={x:+.2f} y={y:+.2f} yaw={yaw:+.2f}"
                        if guid == self.stop_guid:
                            self.stop_acked = True
                    if pending.report_response:
                        result = payload.get("result") if isinstance(payload, dict) else "invalid data"
                        print(f"[响应] guid={guid} result={result}，对应指令={pending.command}", flush=True)
                        if self.debug:
                            print(json.dumps(response, ensure_ascii=False, indent=2), flush=True)
                    self.condition.notify_all()
            except websocket.WebSocketTimeoutException:
                continue
            except (OSError, websocket.WebSocketException) as error:
                self._fail(f"接收底盘响应失败：{error}")
                return

    def check(self):
        with self.condition:
            if self.error is not None:
                raise ProtocolError(self.error)
            if self.pending:
                oldest = next(iter(self.pending.values()))
                if time.monotonic() - oldest.sent_at >= self.response_timeout:
                    raise TimeoutError("等待 response_chassis_move 超时")

    def send(self, command, stopping=False):
        guid = str(uuid.uuid4())
        message = {
            "accid": self.accid,
            "title": "request_chassis_move",
            "guid": guid,
            "timestamp": time.time_ns() // 1_000_000,
            "data": dict(zip(("x", "y", "yaw"), command)),
        }
        report_response = command != self.last_command or stopping
        with self.condition:
            self.pending[guid] = PendingCommand(time.monotonic(), command, report_response)
            if stopping:
                self.stop_guid = guid
                self.stop_acked = False
        self.ws.send(json.dumps(message))
        with self.condition:
            if command != ZERO:
                self.nonzero_sent += 1
            self.last_command = command
        if report_response:
            print(f"[发送] guid={guid} data={json.dumps(message['data'])}", flush=True)
            if self.debug:
                print(json.dumps(message, ensure_ascii=False, indent=2), flush=True)

    def diagnostics(self):
        with self.condition:
            return self.nonzero_sent, self.nonzero_acked, self.last_ack

    def poll_state(self, now):
        """每秒由主循环调用；只发送查询，不等待响应、不改变控制模式。"""
        if not self.debug:
            return
        with self.condition:
            previous_status = ""
            if self.state_pending is not None:
                if now - self.state_pending[1] < 1.0:
                    return
                previous_status = "上次查询超时；"
            guid = str(uuid.uuid4())
            self.state_pending = (guid, now)
            self.state_query_status = previous_status + "等待新查询响应"
        self.ws.send(json.dumps({
            "accid": self.accid,
            "title": "request_chassis_state",
            "guid": guid,
            "timestamp": time.time_ns() // 1_000_000,
            "data": {},
        }))

    def feedback_diagnostics(self, now):
        with self.condition:
            query_status = self.state_query_status
            state = self.state_response
            state_at = self.state_received_at
            info = self.robot_info
            info_at = self.robot_info_received_at
        lines = [f"[底盘查询] {query_status}"]
        for label, response, received_at in (
            ("底盘反馈", state, state_at), ("机器人上报", info, info_at),
        ):
            if received_at is None:
                lines.append(f"[{label}] 尚未收到；不代表速度为零或机器人状态正常")
            else:
                age = max(0.0, now - received_at)
                freshness = "，旧反馈" if age > 2.0 else ""
                lines.append(
                    f"[{label}] 本机收到后 {age:.2f}s{freshness}，原始响应="
                    + json.dumps(response, ensure_ascii=False)
                )
        return lines

    def finish(self):
        """退出前尝试发零，并单独确认这条停止请求的 ACK。"""
        try:
            self.send(ZERO, stopping=True)
            with self.condition:
                acknowledged = self.condition.wait_for(
                    lambda: self.stop_acked, timeout=min(self.response_timeout, 0.5)
                )
            if acknowledged:
                print("退出时的零速度指令已获成功响应；这不代表之前发出过非零运动指令。")
            else:
                print("未收到停止确认，请确认机器人已停下；连接异常时使用实体急停。", file=sys.stderr)
        except (OSError, websocket.WebSocketException) as error:
            print(f"停止指令发送失败：{error}；请使用实体急停。", file=sys.stderr)
        finally:
            self.closed.set()
            if self.receiver is not None:
                self.receiver.join(timeout=0.2)
            sent, acked, _ = self.diagnostics()
            print(f"本次非零运动指令：发送 {sent} 条，收到 success 回复 {acked} 条。")
            if sent == 0:
                print("本次未发送非零运动指令。请点击控制窗口，松开所有方向键，再按住 W。")
            elif acked > 0:
                print("success 不证明底盘已运动；若遥控器能动而接口不动，请核对上层开发者模式、控制源及底盘反馈。")


def run_keyboard(client, args, pygame, screen):
    control = KeyboardControl(args.speed, args.turn_speed)
    stream = MotionStream(client, args.response_timeout, getattr(args, "debug", False))
    key_codes = {
        "w": pygame.K_w, "s": pygame.K_s, "a": pygame.K_a, "d": pygame.K_d,
        "q": pygame.K_q, "e": pygame.K_e, "up": pygame.K_UP, "down": pygame.K_DOWN,
        "left": pygame.K_LEFT, "right": pygame.K_RIGHT, "space": pygame.K_SPACE,
    }
    font = pygame.font.Font(None, 28)
    clock = pygame.time.Clock()
    previous = None
    previous_keyboard = None
    next_send = 0.0
    next_debug = 0.0
    previous_debug_at = None
    previous_counts = (0, 0)
    nonzero_since = None
    try:
        stream.start()
        # 连接建立后先归零；停止行为不依赖窗口是否有焦点。
        stream.send(ZERO)
        while True:
            exit_reason = None
            focus_lost = False
            stop_pressed = False
            for event in pygame.event.get():
                if stream.debug and event.type in (pygame.KEYDOWN, pygame.KEYUP):
                    event_name = "KEYDOWN" if event.type == pygame.KEYDOWN else "KEYUP"
                    print(f"[按键事件] t={time.monotonic():.3f} {event_name} {pygame.key.name(event.key)}", flush=True)
                if event.type == pygame.QUIT:
                    exit_reason = "控制窗口被关闭"
                elif event.type == pygame.KEYDOWN:
                    if event.key == pygame.K_ESCAPE:
                        exit_reason = "按下 Esc"
                    elif event.key == pygame.K_SPACE:
                        stop_pressed = True
                elif event.type in (pygame.WINDOWFOCUSLOST, pygame.WINDOWMINIMIZED):
                    focus_lost = True
            if exit_reason is not None:
                print(f"[退出] {exit_reason}")
                break

            pressed = pygame.key.get_pressed()
            if pressed[pygame.K_LCTRL] or pressed[pygame.K_RCTRL]:
                if pressed[pygame.K_c]:
                    print("[退出] 按下 Ctrl+C")
                    break
            focused = pygame.key.get_focused() and not focus_lost
            keys = {name for name, code in key_codes.items() if pressed[code]}
            if stop_pressed:
                keys.add("space")
            command = control.command(keys, focused)
            now = time.monotonic()
            if command == ZERO:
                nonzero_since = None
            elif nonzero_since is None:
                nonzero_since = now
            stream.check()
            if command != previous or now >= next_send:
                stream.send(command)
                previous = command
                next_send = now + 1.0 / args.rate

            keyboard = (focused, control.armed, tuple(sorted(keys)))
            if keyboard != previous_keyboard:
                if not focused:
                    status_text = "窗口无焦点：点击控制窗口后松开所有方向键"
                elif not control.armed:
                    status_text = "等待松键：先松开空格和所有方向键，再重新按下"
                else:
                    status_text = "键盘就绪"
                print(f"[键盘] {status_text}；按键={','.join(sorted(keys)) or '无'}", flush=True)
                previous_keyboard = keyboard

            if stream.debug and now >= next_debug:
                # 先发送最新运动/停止值，再做低频只读诊断；不调用同步 client.request。
                stream.poll_state(now)
                sent, acked, _ = stream.diagnostics()
                elapsed = 0.0 if previous_debug_at is None else now - previous_debug_at
                held = 0.0 if nonzero_since is None else now - nonzero_since
                print(
                    f"[持续发送] 连续非零={held:.2f}s；本段={elapsed:.2f}s "
                    f"非零发送={sent - previous_counts[0]} success={acked - previous_counts[1]}；"
                    f"累计={sent}/{acked}；目标={command}；焦点={bool(focused)}；"
                    f"按键={','.join(sorted(keys)) or '无'}",
                    flush=True,
                )
                for line in stream.feedback_diagnostics(time.monotonic()):
                    print(line, flush=True)
                previous_counts = (sent, acked)
                previous_debug_at = now
                next_debug = now + 1.0

            screen.fill((24, 29, 38))
            status = "ACTIVE" if focused and control.armed else "STOPPED - release keys, focus window"
            sent, acked, last_ack = stream.diagnostics()
            lines = [
                "TRON2 - hold to move / release to stop",
                "W/S or Up/Down: x forward/back",
                "A/D or Left/Right: yaw left/right",
                "Q/E: y left/right (depends on chassis mode)",
                "Space: stop | Esc / Ctrl+C: stop and exit",
                "Focus lost: stop; release all keys before resuming",
                f"Target: x={command[0]:+.2f}   y={command[1]:+.2f}   yaw={command[2]:+.2f}",
                status,
                f"Mode: {getattr(args, 'mode', None) or 'unchanged (unknown)'} | Nonzero sent/ACK: {sent}/{acked}",
                f"Last ACK: {last_ack}",
            ]
            for index, line in enumerate(lines):
                screen.blit(font.render(line, True, (225, 234, 244)), (20, 20 + index * 36))
            pygame.display.flip()
            clock.tick(100)
    finally:
        stream.finish()


def main():
    args = parse_args()
    try:
        import pygame
    except ImportError:
        print("缺少 pygame，请运行：python3 -m pip install pygame", file=sys.stderr)
        return 1
    summary = (
        f"键盘控制 {args.host}:{args.port}：速度值={args.speed}，转向值={args.turn_speed}，"
        f"模式={args.mode or '保持当前模式'}"
    )
    if not confirm_action(summary, args.yes):
        print("已取消，未发送命令")
        return 0
    try:
        pygame.display.init()
        pygame.font.init()
        screen = pygame.display.set_mode((730, 400))
        pygame.display.set_caption("TRON2 Keyboard Control")
        if pygame.display.get_driver() in ("dummy", "offscreen"):
            raise RuntimeError("键盘控制需要可交互的桌面窗口，不能使用 dummy/offscreen 显示驱动")
        with client_from_args(args) as client:
            print_connected(client)
            if args.mode is not None:
                _, response = client.request(
                    "request_set_chassis_mode", "response_set_chassis_mode",
                    {"move_mode": args.mode},
                )
                print(f"[模式] {args.mode} 设置已获 success 回复")
                if args.debug:
                    print(json.dumps(response, ensure_ascii=False, indent=2), flush=True)
            else:
                print("[模式] 未设置，当前模式未知。需要阿克曼模式时启动参数加 --mode ackerman。")
            print("点击控制窗口：W/S 前后，A/D 转向，Q/E 横移；松开停止，空格停止，Esc 退出。")
            run_keyboard(client, args, pygame, screen)
    except KeyboardInterrupt:
        print("\n已退出键盘控制")
    except (OSError, RuntimeError, websocket.WebSocketException, pygame.error) as error:
        print(f"控制底盘运动失败：{error}", file=sys.stderr)
        return 1
    finally:
        pygame.quit()
    return 0


if __name__ == "__main__":
    sys.exit(main())
