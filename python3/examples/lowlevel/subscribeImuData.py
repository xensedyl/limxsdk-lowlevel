"""
subscribeImuData 接口示例。

函数原型：
    subscribeImuData(
        cb: Callable[[datatypes.ImuData], Any]
    ) -> None

功能：
    订阅机器人的 IMU 数据，并在接收到新的 IMU 数据时调用指定的回调函数。

参数：
    cb：用于处理新 IMU 数据的回调函数。

返回值：
    无
"""

import sys
from functools import partial
import limxsdk.robot.Robot as Robot
import limxsdk.robot.RobotType as RobotType
import limxsdk.datatypes as datatypes

class RobotReceiver:
    # 订阅机器人的 IMU数据
    def imuDataCallback(self, imu: datatypes.ImuData):
        print("\n------\nrobot_state:" + \
              "\n  stamp: " + str(imu.stamp) + \
              "\n  acc: " + str(imu.acc) + \
              "\n  gyro: " + str(imu.gyro) + \
              "\n  quat: " + str(imu.quat))

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
    imuDataCallback = partial(receiver.imuDataCallback)

    # 订阅机器人IMU数据
    robot.subscribeImuData(imuDataCallback)
    
    # 休眠 1 秒，防止程序退出    
    import time
    while True:
        time.sleep(1) 