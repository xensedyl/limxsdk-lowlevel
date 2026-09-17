"""ServoJ：双臂和头部 16 维关节高频伺服示例。

官方要求在实时系统中以不低于 500 Hz 的频率发送 ServoJ 指令。
普通 Python/Linux 不能保证硬实时，本脚本仅用于接口演示。
"""

import math
import time

from example_common import get_servoj_joints, wait_until_ready
from robot_utils import Tron2, Tron2Config


ROBOT_IP = "10.192.1.2"
SERVO_RATE = 1000.0
RUN_TIME = 5.0
AMPLITUDE = 0.3  # rad
FREQUENCY = 1  # Hz
CONTROL_JOINT = 6  # 左臂 wrist_roll_L_Joint


def main():
    config = Tron2Config(robot_ip=ROBOT_IP, servoj_rate=SERVO_RATE)
    with Tron2(config) as robot:
        wait_until_ready(robot)
        state = robot.get_joint_states(timeout=2.0)
        initial_q = get_servoj_joints(state)

        print(f"机器人已连接：{robot.accid}")
        print(f"ServoJ 将以 {SERVO_RATE:.0f} Hz 运行 {RUN_TIME:.1f} 秒")
        input("确认机器人周围安全后按 Enter 开始，按 Ctrl+C 取消：")

        robot.set_servoj_mode()
        start = time.monotonic()
        try:
            while True:
                elapsed = time.monotonic() - start
                if elapsed >= RUN_TIME:
                    break
                target_q = initial_q.copy()
                target_q[CONTROL_JOINT] += AMPLITUDE * math.sin(
                    2.0 * math.pi * FREQUENCY * elapsed
                )
                robot.servoj(target_q)
        finally:
            robot.set_movej_mode()

        print("ServoJ 示例执行完成")


if __name__ == "__main__":
    main()
