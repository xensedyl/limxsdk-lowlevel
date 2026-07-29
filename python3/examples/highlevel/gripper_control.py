"""逐际二指夹爪控制指令测试脚本。"""

import json
import time

from example_common import wait_until_ready
from robot_utils import StateError, Tron2, Tron2Config


ROBOT_IP = "10.192.1.2"

# 取值范围均为 0～96；opening=0 为闭合，opening=96 为完全张开。
# 开口度，0-100，无量纲（0对应最小闭合，100对应张开到最大)
# 夹爪速度，0~100 无量纲（数值越大速度越快）
# 力，夹爪夹持力，0~100 无单位（数值越大力越大）
LEFT_OPENING = 0
LEFT_SPEED = 100
LEFT_FORCE = 1
RIGHT_OPENING = 0
RIGHT_SPEED = 100
RIGHT_FORCE = 1


def main():
    with Tron2(Tron2Config(robot_ip=ROBOT_IP)) as robot:
        wait_until_ready(robot)
        print(f"机器人已连接：{robot.accid}")
        print(
            f"左夹爪 opening={LEFT_OPENING}, speed={LEFT_SPEED}, force={LEFT_FORCE}"
        )
        print(
            f"右夹爪 opening={RIGHT_OPENING}, speed={RIGHT_SPEED}, force={RIGHT_FORCE}"
        )
        input("确认夹爪内无人员或异物后按 Enter 发送，按 Ctrl+C 取消：")

        robot.set_gripper(
            left_opening=LEFT_OPENING,
            left_speed=LEFT_SPEED,
            left_force=LEFT_FORCE,
            right_opening=RIGHT_OPENING,
            right_speed=RIGHT_SPEED,
            right_force=RIGHT_FORCE,
        )

        deadline = time.monotonic() + 5.0
        last_state = None
        while time.monotonic() < deadline:
            try:
                last_state = robot.get_gripper_state(timeout=1.0)
            except StateError:
                continue

            left_reached = abs(last_state["left_opening"] - LEFT_OPENING) <= 2
            right_reached = abs(last_state["right_opening"] - RIGHT_OPENING) <= 2
            if left_reached and right_reached:
                print("夹爪已到达目标开口度：")
                print(json.dumps(last_state, ensure_ascii=False, indent=2))
                return

        raise StateError(f"夹爪等待到位超时，最后状态：{last_state}")


if __name__ == "__main__":
    main()
