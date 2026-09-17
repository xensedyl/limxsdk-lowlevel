"""ServoJ：双臂和头部 16 维关节高频伺服示例。

官方要求在实时系统中按 300 Hz 的控制频率发送 ServoJ 指令。
普通 Python/Linux 不能保证硬实时，本脚本仅用于接口演示。

关于抖动：ServoJ 无插值，下发什么就跟什么，因此指令的速度/加速度必须在
关节能力范围内，否则电机饱和、结构共振，表现为明显抖动。参考量级：
    峰值角速度     = AMPLITUDE * 2*pi*FREQUENCY
    峰值角加速度   = AMPLITUDE * (2*pi*FREQUENCY)**2
把 FREQUENCY 控制在 1 Hz 以内、并让峰值速度不超过约 1 rad/s 是安全起点。
"""

import math
import time

from example_common import get_servoj_joints, wait_until_ready
from robot_utils import Tron2, Tron2Config


ROBOT_IP = "10.192.1.2"
SERVO_RATE = 300.0
RUN_TIME = 10.0
AMPLITUDE = 0.3  # rad
FREQUENCY = 0.3  # Hz -> 峰值速度约 0.57 rad/s，峰值加速度约 1.07 rad/s^2
RAMP_TIME = 1.5  # 幅度渐入/渐出时间(秒)，避免起停冲击
FILTER_RATIO = 0.3  # 机器人侧一阶滤波，1.0=无滤波；越小越平滑、滞后越大
CONTROL_JOINT = 6  # 左臂 wrist_roll_L_Joint


def amplitude_envelope(elapsed: float) -> float:
    """两端余弦渐变的幅度包络，取值 0~1。

    让正弦幅度从 0 平滑涨到 AMPLITUDE 再平滑回落，
    避免 ServoJ 起停瞬间出现速度阶跃。
    """
    ramp = min(RAMP_TIME, RUN_TIME / 2.0)
    if ramp <= 0.0:
        return 1.0
    if elapsed < ramp:
        phase = elapsed / ramp
    elif elapsed > RUN_TIME - ramp:
        phase = (RUN_TIME - elapsed) / ramp
    else:
        return 1.0
    phase = max(0.0, min(1.0, phase))
    return 0.5 * (1.0 - math.cos(math.pi * phase))


def main():
    config = Tron2Config(
        robot_ip=ROBOT_IP,
        servoj_rate=SERVO_RATE,
        servoj_filter_ratio=FILTER_RATIO,
    )
    with Tron2(config) as robot:
        wait_until_ready(robot)
        state = robot.get_joint_states(timeout=2.0)
        initial_q = get_servoj_joints(state)

        peak_vel = AMPLITUDE * 2.0 * math.pi * FREQUENCY
        peak_acc = AMPLITUDE * (2.0 * math.pi * FREQUENCY) ** 2
        print(f"机器人已连接：{robot.accid}")
        print(f"ServoJ 将以 {SERVO_RATE:.0f} Hz 运行 {RUN_TIME:.1f} 秒")
        print(f"轨迹峰值：速度 {peak_vel:.2f} rad/s，加速度 {peak_acc:.2f} rad/s^2")
        input("确认机器人周围安全后按 Enter 开始，按 Ctrl+C 取消：")

        robot.set_servoj_mode()
        total_ticks = int(RUN_TIME * SERVO_RATE)
        try:
            # 相位按 tick 计数推进而非读挂钟：单次发送延迟不会让目标点跳变，
            # 从而不把时序抖动放大成位置台阶。
            for tick in range(total_ticks + 1):
                elapsed = tick / SERVO_RATE
                offset = (
                    AMPLITUDE
                    * amplitude_envelope(elapsed)
                    * math.sin(2.0 * math.pi * FREQUENCY * elapsed)
                )
                target_q = initial_q.copy()
                target_q[CONTROL_JOINT] += offset
                robot.servoj(target_q)

            # 包络已归零，此处再显式回到初始姿态，确保切模式前无残留偏移
            robot.servoj(initial_q)
        finally:
            robot.set_movej_mode()

        print("ServoJ 示例执行完成")


if __name__ == "__main__":
    main()
