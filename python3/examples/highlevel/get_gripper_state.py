"""获取逐际二指夹爪完整状态的测试脚本。"""

import json

from example_common import wait_until_ready
from robot_utils import Tron2, Tron2Config


ROBOT_IP = "10.192.1.2"


def main():
    with Tron2(Tron2Config(robot_ip=ROBOT_IP)) as robot:
        wait_until_ready(robot)
        state = robot.get_gripper_state(timeout=2.0)

        print("夹爪状态（opening、speed、force 的范围均为 0～100）：")
        print(json.dumps(state, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
