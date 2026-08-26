"""MoveP + MoveH 未到位时发送下一目标的打断测试。"""

import time

from example_common import get_ee_pose_lists, wait_until_ready
from robot_utils import CommandError, MotionMode, Tron2, Tron2Config


ROBOT_IP = "10.192.1.2"
MOVE_TIME = 0.1
INTERRUPT_AFTER = 0.05
LEFT_Z_OFFSET = 0.02
LEFT_Z_RANGE = (-0.673, 0.5)

# [pitch, yaw]，单位为 rad。
FIRST_HEAD = [0.0, 0.3]
SECOND_HEAD = [0.3, 0.0]


def send_movep_moveh(robot, arm_target, head_target):
    """发送一组同周期的 MoveP 和 MoveH 命令。"""
    robot.movep(arm_target, move_time=MOVE_TIME)

    # MoveP 和 MoveH 都使用插值模式。临时同步本地模式状态，避免
    # move_head() 再次发送模式切换命令而打断刚发送的 MoveP。
    saved_mode = robot.motion_mode
    robot.motion_mode = MotionMode.MOVEJ
    try:
        robot.move_head(head_target, move_time=MOVE_TIME)
    finally:
        robot.motion_mode = saved_mode


def main():
    if not 0.0 < INTERRUPT_AFTER < MOVE_TIME:
        raise ValueError("INTERRUPT_AFTER 必须大于 0 且小于 MOVE_TIME")

    with Tron2(Tron2Config(robot_ip=ROBOT_IP)) as robot:
        wait_until_ready(robot)
        ee_pose = robot.get_ee_poses(timeout=2.0)
        initial_left, initial_right = get_ee_pose_lists(ee_pose)

        first_left = initial_left.copy()
        first_left[2] += LEFT_Z_OFFSET
        if not LEFT_Z_RANGE[0] <= first_left[2] <= LEFT_Z_RANGE[1]:
            raise CommandError(
                f"第一目标 z={first_left[2]:.3f} m 超出范围 {LEFT_Z_RANGE}"
            )

        first_arm_target = first_left + initial_right
        second_arm_target = initial_left + initial_right

        print(f"机器人已连接：{robot.accid}")
        print(f"第一目标：左臂 z 增加 {LEFT_Z_OFFSET:.3f} m，头部 {FIRST_HEAD}")
        print(f"{INTERRUPT_AFTER:.1f} 秒后打断并发送第二目标：回到初始位姿，头部 {SECOND_HEAD}")
        input("确认机器人周围安全后按 Enter 开始，按 Ctrl+C 取消：")

        start = time.monotonic()
        send_movep_moveh(robot, first_arm_target, FIRST_HEAD)
        print(f"[{time.monotonic() - start:.3f}s] 第一目标已发送")

        time.sleep(INTERRUPT_AFTER)
        send_movep_moveh(robot, second_arm_target, SECOND_HEAD)
        print(f"[{time.monotonic() - start:.3f}s] 第二目标已发送（打断测试）")

        time.sleep(MOVE_TIME + 0.5)
        print("测试结束，请确认机器人是否直接转向第二目标")


if __name__ == "__main__":
    main()
