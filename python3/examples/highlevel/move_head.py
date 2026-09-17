"""MoveH：头部 pitch、yaw 插值运动示例。"""

import time

from example_common import wait_until_ready
from robot_utils import Tron2, Tron2Config


ROBOT_IP = "10.192.1.2"
MOVE_TIME = 3.0

# [pitch, yaw]，单位为 rad。
# pitch 范围 [-0.78, 1.04]，yaw 范围 [-1.57, 1.57]。
TARGET_HEAD = [0.0, 0.0]


def main():
    with Tron2(Tron2Config(robot_ip=ROBOT_IP)) as robot:
        wait_until_ready(robot)
        print(f"机器人已连接：{robot.accid}")
        print(f"MoveH 目标关节：{TARGET_HEAD}")
        input("确认机器人周围安全后按 Enter 发送，按 Ctrl+C 取消：")

        robot.move_head(TARGET_HEAD, move_time=MOVE_TIME)
        time.sleep(MOVE_TIME + 0.5)
        print("MoveH 命令执行结束")


if __name__ == "__main__":
    main()
