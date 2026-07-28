"""
publishRobotCmd 接口示例。

函数原型：
    publishRobotCmd(
        cmd: datatypes.RobotCmd
    ) -> bool

功能：
    发布一个命令来控制机器人的动作。

参数：
    cmd：表示所需机器人命令的 datatypes.RobotCmd 对象。

返回值：
    成功返回 True，失败返回 False。
备注：
    无
"""
import sys
import time
import limxsdk.robot.Rate as Rate
import limxsdk.robot.Robot as Robot
import limxsdk.robot.RobotType as RobotType
import limxsdk.datatypes as datatypes

TARGET_Q = [
    0.0,
    0.0,
    0.0,
    0.0,
    0.0,
    0.0,
    0.0,
    0.0,
    0.0,
    0.0,
    0.0,
    0.0,
    0.0,
    0.0,
    0.0,
    0.0,
]

if __name__ == '__main__':
    # 创建一个 Robot 实例
    robot = Robot(RobotType.Tron2)

    # 对于仿真，通常设置为 "127.0.0.1"，而对于真实机器人，设置为 "10.192.1.2"
    robot_ip = "10.192.1.2"
    # 检查是否提供了机器人 IP 的命令行参数
    if len(sys.argv) > 1:
        robot_ip = sys.argv[1]

    # 使用 robot_ip 初始化机器人
    if not robot.init(robot_ip):
        sys.exit()

    # 获取电机数量信息
    motor_number = robot.getMotorNumber()
    if motor_number != 16:
        raise RuntimeError(f"当前示例要求 16 个电机，实际为 {motor_number} 个")
    print(f"电机数量: {motor_number}")
    
    # 主循环以连续发布机器人命令
    rate = Rate(300) # 300Hz
    cmd_msg = datatypes.RobotCmd()
    while True:
        # 设置时间戳、控制模式、关节位置、速度、力矩、Kp 和 Kd 的默认值
        # motor_names 对应您要控制的关节名称
        # 注意以下仅为格式示例，在实际使用过程中应填充具体参数
        cmd_msg.stamp = time.time_ns()
        cmd_msg.mode = [0.0 for _ in range(motor_number)]  # 实际使用无需改变
        cmd_msg.q = TARGET_Q.copy()  # 目标关节角度，控制频率 300 Hz
        cmd_msg.dq = [0.0 for _ in range(motor_number)]  # 实际使用无需改变
        cmd_msg.tau = [0.0 for _ in range(motor_number)]  # 实际使用无需改变
        # 顺序：左臂 7 个、右臂 7 个、头部 pitch 和 yaw。
        cmd_msg.Kp = [
            420, 420, 300, 300, 200, 200, 200,
            420, 420, 300, 300, 200, 200, 200,
            10, 10,
        ]
        cmd_msg.Kd = [
            12, 12, 15, 15, 10, 10, 10,
            12, 12, 15, 15, 10, 10, 10,
            3, 3,
        ]
        cmd_msg.motor_names = ["" for _ in range(motor_number)]  # 实际使用无需改变
        robot.publishRobotCmd(cmd_msg)  # 发布    机器人命令
        rate.sleep()  # 控制循环频率