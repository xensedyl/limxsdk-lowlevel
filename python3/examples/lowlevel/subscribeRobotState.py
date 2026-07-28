"""subscribeRobotState 接口示例。

函数原型：
    subscribeRobotState(
        callback: Callable[[datatypes.RobotState], Any]
    ) -> bool

功能：
    订阅机器人状态。收到状态更新时，SDK 将调用指定的回调函数。

参数：
    callback：机器人状态回调函数，参数类型为 datatypes.RobotState。

返回值：
    订阅成功返回 True，否则返回 False。
"""
import sys
from functools import partial
import limxsdk.robot.Robot as Robot
import limxsdk.robot.RobotType as RobotType
import limxsdk.datatypes as datatypes

class RobotReceiver:
    # 用于接收机器人状态的回调函数
    """
    robot state q, dq, tau关节顺序:
    | 索引 | 关节
    |  0  | abad_L_Joint
    |  1  | hip_L_Joint
    |  2  | yaw_L_Joint
    |  3  | knee_L_Joint
    |  4  | wrist_yaw_L_Joint
    |  5  | wrist_pitch_L_Joint
    |  6  | wrist_roll_L_Joint
    |  7  | abad_R_Joint
    |  8  | hip_R_Joint
    |  9  | yaw_R_Joint
    | 10  | knee_R_Joint
    | 11  | wrist_yaw_R_Joint
    | 12  | wrist_pitch_R_Joint
    | 13  | wrist_roll_R_Joint
    | 14  | head_pitch_Joint
    | 15  | head_yaw_Joint
    """
    def robotStateCallback(self, robot_state: datatypes.RobotState):
        print("\n------\nrobot_state:" + \
              "\n  stamp: " + str(robot_state.stamp) + \
              "\n  tau: " + str(robot_state.tau) + \
              "\n  q: " + str(robot_state.q) + \
              "\n  dq: " + str(robot_state.dq))

if __name__ == '__main__':
    # 创建一个类型为Tron2的Robot实例
    robot = Robot(RobotType.Tron2)

    robot_ip = "10.192.1.2"
    # 检查是否提供了机器人 IP 的命令行参数
    if len(sys.argv) > 1:
        robot_ip = sys.argv[1]

    # 使用 robot_ip 初始化机器人
    if not robot.init(robot_ip):
        sys.exit()

    # 创建一个 RobotReceiver 实例来处理回调
    receiver = RobotReceiver()

    # 创建回调函数的 partial 函数
    robotStateCallback = partial(receiver.robotStateCallback)

    # 订阅机器人状态
    robot.subscribeRobotState(robotStateCallback)
    
    # 休眠 1 秒，防止程序退出
    import time
    while True:
        time.sleep(1) 