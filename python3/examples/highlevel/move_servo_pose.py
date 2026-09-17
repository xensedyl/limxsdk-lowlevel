"""ServoP：双臂末端位姿连续伺服示例。"""

import math
import time

from example_common import get_ee_pose_lists, wait_until_ready
from robot_utils import Tron2, Tron2Config


ROBOT_IP = "10.192.1.2"
SERVO_RATE = 1000.0
RUN_TIME = 10.0
AMPLITUDE = 0.02  # m
FREQUENCY = 0.5  # Hz
LEFT_Z_RANGE = (-0.673, 0.5)


def main():
    config = Tron2Config(robot_ip=ROBOT_IP, servop_rate=SERVO_RATE)
    with Tron2(config) as robot:
        wait_until_ready(robot)
        ee_pose = robot.get_ee_poses(timeout=2.0)
        initial_left, initial_right = get_ee_pose_lists(ee_pose)

        if not (
            LEFT_Z_RANGE[0] + AMPLITUDE
            <= initial_left[2]
            <= LEFT_Z_RANGE[1] - AMPLITUDE
        ):
            raise ValueError("当前左臂 z 位置过于接近限位，不能执行示例轨迹")

        print(f"机器人已连接：{robot.accid}")
        print(f"ServoP 将以 {SERVO_RATE:.0f} Hz 运行 {RUN_TIME:.1f} 秒")
        input("确认机器人周围安全后按 Enter 开始，按 Ctrl+C 取消：")

        robot.set_servop_mode()
        start = time.monotonic()
        try:
            while True:
                elapsed = time.monotonic() - start
                if elapsed >= RUN_TIME:
                    break
                left_pose = initial_left.copy()
                left_pose[2] += AMPLITUDE * math.sin(
                    2.0 * math.pi * FREQUENCY * elapsed
                )
                robot.servop(left_pose, initial_right)
        finally:
            robot.set_movep_mode()

        print("ServoP 示例执行完成")


if __name__ == "__main__":
    main()
