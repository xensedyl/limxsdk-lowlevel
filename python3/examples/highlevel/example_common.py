"""TRON2 上层接口示例的公共辅助函数。"""

import time
from typing import Dict, List, Tuple

from robot_utils import ConnectionError, JointIndex, Tron2


JOINT_STATE_NAMES = [
    "abad_L_Joint",
    "hip_L_Joint",
    "yaw_L_Joint",
    "knee_L_Joint",
    "wrist_yaw_L_Joint",
    "wrist_pitch_L_Joint",
    "wrist_roll_L_Joint",
    "left_gripper",
    "abad_R_Joint",
    "hip_R_Joint",
    "yaw_R_Joint",
    "knee_R_Joint",
    "wrist_yaw_R_Joint",
    "wrist_pitch_R_Joint",
    "wrist_roll_R_Joint",
    "right_gripper",
    "head_pitch_Joint",
    "head_yaw_Joint",
]


def wait_until_ready(robot: Tron2, timeout: float = 5.0) -> None:
    """等待 WebSocket 建连并收到包含 ACCID 的机器人消息。"""
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if robot.is_connected() and robot.accid:
            return
        time.sleep(0.05)
    raise ConnectionError(f"等待机器人连接或 ACCID 超时（{timeout} 秒）")


def get_arm_joints(state: Dict) -> List[float]:
    """从 18 维状态中提取双臂 14 维关节角。"""
    values = state["states"]
    return values[JointIndex.LEFT_ARM] + values[JointIndex.RIGHT_ARM]


def get_servoj_joints(state: Dict) -> List[float]:
    """从 18 维状态中提取 ServoJ 所需的双臂和头部 16 维关节角。"""
    values = state["states"]
    return (
        values[JointIndex.LEFT_ARM]
        + values[JointIndex.RIGHT_ARM]
        + values[JointIndex.HEAD]
    )


def get_ee_pose_lists(ee_pose: Dict) -> Tuple[List[float], List[float]]:
    """将末端状态转换为左右臂各 7 维的 xyz+wxyz 位姿。"""
    left_pose = ee_pose["left_position"] + ee_pose["left_quat"]
    right_pose = ee_pose["right_position"] + ee_pose["right_quat"]
    return left_pose, right_pose
