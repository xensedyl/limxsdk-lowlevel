"""MoveJ：双臂 14 维关节空间插值运动示例。"""

from example_common import wait_until_ready
from robot_utils import Tron2, Tron2Config


ROBOT_IP = "10.192.1.2"
MOVE_TIME = 5.0

# 顺序为左臂 7 关节、右臂 7 关节，单位为 rad。
TARGET_JOINTS = [
    0.026899, 0.2612, -0.02709991, -1.5477003,  0.265, 0.0180999 , -0.0614999,
    0.008999, -0.269,  0.02069998, -1.5567001, -0.254, -0.02309972, 0.06469989,
]

# TARGET_JOINTS = [
#     0.0, 0.0, 0.0, 0.0,  0.0, 0.0 , 0.0,
#     0.0, 0.0,  0.0, 0.0, 0.0, 0.0 , 0.0,
# ]

def main():
    with Tron2(Tron2Config(robot_ip=ROBOT_IP)) as robot:
        wait_until_ready(robot)
        print(f"机器人已连接：{robot.accid}")
        print(f"MoveJ 目标关节：{TARGET_JOINTS}")
        input("确认机器人周围安全后按 Enter 发送，按 Ctrl+C 取消：")

        robot.movej(TARGET_JOINTS, move_time=MOVE_TIME)
        if robot.wait_until_reached(
            TARGET_JOINTS,
            tolerance=0.05,
            timeout=MOVE_TIME + 5.0,
        ):
            print("MoveJ 执行完成")
        else:
            print("MoveJ 等待到位超时，请检查机器人状态")


if __name__ == "__main__":
    main()
