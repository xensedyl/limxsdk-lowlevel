"""
subscribeDiagnosticValue 接口示例。

函数原型：
    subscribeDiagnosticValue(
        callback: Callable[[datatypes.DiagnosticValue], Any]
    ) -> bool

功能：
    在真机部署中，该方法用于订阅机器人的诊断值和状态信息。当机器人发出诊断值时，系统会调用指定的回调函数，并传递包含诊断值的 datatypes.DiagnosticValue 结构体对象给回调函数进行处理。这可以帮助实时监控机器人的健康状态，并及时做出反应以处理可能的问题。

参数：
    callback：用于接收机器人诊断值的回调函数，其参数类型为 datatypes.DiagnosticValue。datatypes.DiagnosticValue 结构体包含机器人诊断值的信息，包括时间戳、级别、名称、代码和消息字段。

返回值：
    成功返回 True，失败返回 False。
备注：
    无
"""

import sys
from functools import partial
import limxsdk.robot.Robot as Robot
import limxsdk.robot.RobotType as RobotType
import limxsdk.datatypes as datatypes

class RobotReceiver:
    # 用于接收诊断值的回调函数
    def diagnosticValueCallback(self, diagnostic_value: datatypes.DiagnosticValue):
        print("\n------\ndiagnostic_value:" + \
              "\n  stamp: " + str(diagnostic_value.stamp) + \
              "\n  name: " + diagnostic_value.name + \
              "\n  level: " + str(diagnostic_value.level) + \
              "\n  code: " + str(diagnostic_value.code) + \
              "\n  message: " + diagnostic_value.message)

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
    diagnosticValueCallback = partial(receiver.diagnosticValueCallback)

    # 订阅机器人诊断信息
    robot.subscribeDiagnosticValue(diagnosticValueCallback)
    
    # 休眠 1 秒，防止程序退出
    import time
    while True:
        time.sleep(1) 