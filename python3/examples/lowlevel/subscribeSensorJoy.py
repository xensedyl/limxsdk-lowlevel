"""subscribeSensorJoy 接口示例。

函数原型：
    subscribeSensorJoy(
        callback: Callable[[datatypes.SensorJoy], Any]
    ) -> bool

功能：
    在真机部署中，该方法用于订阅来自机器人遥控器的数据。当机器人接收到遥控器数据时，将会调用指定的回调函数，并传递包含遥控器数据的 datatypes.SensorJoy 结构体对象给回调函数进行处理。

参数：
    callback：表示回调函数，用于接收机器人遥控器的数据。回调函数的参数类型为 datatypes.SensorJoy。

返回值：
    成功返回 True，失败返回 False。
"""

import sys
import time
from functools import partial
import limxsdk.robot.Robot as Robot
import limxsdk.robot.RobotType as RobotType
import limxsdk.datatypes as datatypes

class RobotReceiver:
    # 用于接收遥控器数据的回调函数
    def sensorJoyCallback(self, sensor_joy: datatypes.SensorJoy):
        print("\n------\nsensor_joy:" + \
              "\n  stamp: " + str(sensor_joy.stamp) + \
              "\n  axes: " + str(sensor_joy.axes) + \
              "\n  buttons: " + str(sensor_joy.buttons))

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
    sensorJoyCallback = partial(receiver.sensorJoyCallback)

    # 订阅机器人遥控数据
    robot.subscribeSensorJoy(sensorJoyCallback)
    
    # 休眠 1 秒，防止程序退出
    import time
    while True:
        time.sleep(1) 
    