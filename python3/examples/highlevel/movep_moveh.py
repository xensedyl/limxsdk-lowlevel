"""MoveP + MoveH：双臂笛卡尔插值与头部插值联合运动示例。"""

import time

from example_common import get_ee_pose_lists, wait_until_ready
from robot_utils import CommandError, MotionMode, Tron2, Tron2Config


ROBOT_IP = "10.192.1.2"
MOVE_TIME = 3.0
LEFT_Z_OFFSET = 0.02  # 左臂末端沿基座坐标系 z 轴移动 2 cm。
LEFT_Z_RANGE = (-0.673, 0.5)

# [pitch, yaw]，单位为 rad。
# pitch 范围 [-0.78, 1.04]，yaw 范围 [-1.57, 1.57]。
TARGET_HEAD = [0.0, 0.0]


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
        print(f"头部目标关节 [pitch, yaw]：{TARGET_HEAD}")
        input("确认机器人周围安全后按 Enter 发送，按 Ctrl+C 取消：")

        # 先发 MoveP；再发 MoveH 时临时标记为 MOVEJ，避免内部再次切模式打断臂运动。
        robot.movep(left_pose + right_pose, move_time=MOVE_TIME)
        saved_mode = robot.motion_mode
        robot.motion_mode = MotionMode.MOVEJ
        robot.move_head(TARGET_HEAD, move_time=MOVE_TIME)
        robot.motion_mode = saved_mode

        time.sleep(MOVE_TIME + 0.5)
        print("MoveP + MoveH 联合运动执行结束")


if __name__ == "__main__":
    main()
