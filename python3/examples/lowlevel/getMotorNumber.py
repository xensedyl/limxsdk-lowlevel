"""
getMotorNumber 接口示例。

函数原型：
    getMotorNumber() -> uint32_t

功能：
    获取机器人的电机数量。

参数：
    无

返回值：
    返回一个无符号整数，表示机器人中的总电机数量。
备注：
    例如，双轮足形态下的电机数量为 10 个。
"""

import sys
import limxsdk.robot.Robot as Robot
import limxsdk.robot.RobotType as RobotType

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

    # 获取机器人中的电机数量
    motor_number = robot.getMotorNumber()
    print(f"机器人中的电机数量: {motor_number}")