"""获取 TRON2 双臂末端位姿示例。"""

import json

from example_common import wait_until_ready
from robot_utils import Tron2, Tron2Config


ROBOT_IP = "10.192.1.2"


def main():
    with Tron2(Tron2Config(robot_ip=ROBOT_IP)) as robot:
        wait_until_ready(robot)
        ee_pose = robot.get_ee_poses(timeout=2.0)

        print("双臂末端位姿（位置单位：m，四元数顺序：wxyz）：")
        print(json.dumps(ee_pose, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
