"""MoveP：双臂末端笛卡尔空间插值运动示例。"""

import time

from example_common import get_ee_pose_lists, wait_until_ready
from robot_utils import CommandError, Tron2, Tron2Config


ROBOT_IP = "10.192.1.2"
MOVE_TIME = 3.0
LEFT_Z_OFFSET = 0.02  # 左臂末端沿基座坐标系 z 轴移动 2 cm。
LEFT_Z_RANGE = (-0.673, 0.5)


def main():
    with Tron2(Tron2Config(robot_ip=ROBOT_IP)) as robot:
        wait_until_ready(robot)
        ee_pose = robot.get_ee_poses(timeout=2.0)
        left_pose, right_pose = get_ee_pose_lists(ee_pose)

        target_z = left_pose[2] + LEFT_Z_OFFSET
        if not LEFT_Z_RANGE[0] <= target_z <= LEFT_Z_RANGE[1]:
            raise CommandError(f"左臂目标 z={target_z:.3f} m 超出范围 {LEFT_Z_RANGE}")
        left_pose[2] = target_z

        print(f"机器人已连接：{robot.accid}")
        print(f"左臂目标位姿 xyz+wxyz：{left_pose}")
        print(f"右臂保持当前位姿：{right_pose}")
        input("确认机器人周围安全后按 Enter 发送，按 Ctrl+C 取消：")

        robot.movep(left_pose + right_pose, move_time=MOVE_TIME)
        time.sleep(MOVE_TIME + 0.5)
        print("MoveP 命令执行结束")


if __name__ == "__main__":
    main()
