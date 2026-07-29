"""获取 TRON2 双臂、夹爪和头部关节位置示例。"""

from example_common import JOINT_STATE_NAMES, wait_until_ready
from robot_utils import JointIndex, Tron2, Tron2Config


ROBOT_IP = "10.192.1.2"


def main():
    with Tron2(Tron2Config(robot_ip=ROBOT_IP)) as robot:
        wait_until_ready(robot)
        state = robot.get_joint_states(timeout=2.0)
        values = state["states"]

        print(f"timestamp: {state['timestamp']}")
        for index, (name, value) in enumerate(zip(JOINT_STATE_NAMES, values)):
            unit = "ratio" if index in (
                JointIndex.LEFT_GRIPPER,
                JointIndex.RIGHT_GRIPPER,
            ) else "rad"
            print(f"[{index:02d}] {name:<24} {value: .6f} {unit}")


if __name__ == "__main__":
    main()
