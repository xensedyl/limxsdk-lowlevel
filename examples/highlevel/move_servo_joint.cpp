// ServoJ：双臂和头部 16 维关节高频伺服。
//
// 官方文档 §3.6.4.1 要求在实时系统中按 300 Hz 的控制频率发送 ServoJ 指令。
// 普通 Linux 线程调度不保证硬实时，本示例仅用于接口演示。
//
// 关于抖动：ServoJ 无插值，下发什么就跟什么，因此指令的速度/加速度必须在
// 关节能力范围内，否则电机饱和、结构共振，表现为明显抖动。参考量级：
//     峰值角速度   = AMPLITUDE * 2*pi*FREQUENCY
//     峰值角加速度 = AMPLITUDE * (2*pi*FREQUENCY)^2
// 把 FREQUENCY 控制在 1 Hz 以内、峰值速度不超过约 1 rad/s 是安全起点。
#include "robot_utils.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <thread>

namespace {
constexpr double PI = 3.14159265358979323846;

constexpr double SERVO_RATE    = 300.0;  // Hz，官方推荐的控制频率
constexpr double RUN_TIME      = 10.0;   // 秒
constexpr double AMPLITUDE     = 0.3;    // rad
constexpr double FREQUENCY     = 0.3;    // Hz → 峰值速度约 0.57 rad/s，加速度约 1.07 rad/s^2
constexpr double RAMP_TIME     = 1.5;    // 秒，幅度渐入/渐出，避免起停冲击
constexpr double FILTER_RATIO  = 0.3;    // 0~1，1.0 表示无滤波；越小越平滑、滞后越大
constexpr std::size_t CONTROL_JOINT = 6; // 左臂 wrist_roll_L_Joint

static_assert(SERVO_RATE >= 300.0 && SERVO_RATE <= 2000.0, "SERVO_RATE 应为 300～2000 Hz");
static_assert(FILTER_RATIO > 0.0 && FILTER_RATIO <= 1.0, "FILTER_RATIO 应在 (0,1]");
static_assert(CONTROL_JOINT < 16, "CONTROL_JOINT 应为 0～15");
static_assert(RUN_TIME > 0.0 && AMPLITUDE >= 0.0 && FREQUENCY > 0.0, "伺服参数无效");

// 两端余弦渐变的幅度包络，取值 0~1。
// 让正弦幅度从 0 平滑涨到 AMPLITUDE 再平滑回落，避免 ServoJ 起停瞬间的速度阶跃。
double amplitude_envelope(double elapsed) {
    const double ramp = std::min(RAMP_TIME, RUN_TIME / 2.0);
    if (ramp <= 0.0) return 1.0;
    double phase;
    if (elapsed < ramp) phase = elapsed / ramp;
    else if (elapsed > RUN_TIME - ramp) phase = (RUN_TIME - elapsed) / ramp;
    else return 1.0;
    phase = std::min(1.0, std::max(0.0, phase));
    return 0.5 * (1.0 - std::cos(PI * phase));
}
} // namespace

int main(int argc, char** argv) {
    using namespace tron2;
    return guard([&] {
        Options args(argc, argv, connection_values());
        if (args.has("help")) {
            print_connection_help("move_servo_joint [选项]",
                {"频率、幅值、时长和滤波系数是源码顶部的常量，修改后重新编译",
                 "抖动明显时优先降低 FREQUENCY，其次调小 FILTER_RATIO",
                 "普通 Linux 不保证硬实时；无逐条到位确认"});
            return 0;
        }
        require(args.positional.empty(), "此示例不接受位置参数");
        auto client = connect(args);
        const auto initial = numbers(
            client->request("request_get_joint_state", Json::object(), false).at("data").at("q"),
            16, "q");

        // 头部两个关节有硬限位，轨迹整体必须落在范围内。
        if (CONTROL_JOINT == 14)
            require(initial[14] - AMPLITUDE >= HEAD_PITCH_MIN &&
                    initial[14] + AMPLITUDE <= HEAD_PITCH_MAX, "pitch 轨迹超限");
        if (CONTROL_JOINT == 15)
            require(initial[15] - AMPLITUDE >= HEAD_YAW_MIN &&
                    initial[15] + AMPLITUDE <= HEAD_YAW_MAX, "yaw 轨迹超限");

        const double peak_velocity = AMPLITUDE * 2.0 * PI * FREQUENCY;
        const double peak_acceleration = AMPLITUDE * std::pow(2.0 * PI * FREQUENCY, 2);
        std::cout << "机器人已连接：" << client->accid() << std::endl;
        std::cout << "ServoJ 将以 " << SERVO_RATE << " Hz 运行 " << RUN_TIME << " 秒，"
                  << "控制关节 " << CONTROL_JOINT << std::endl;
        std::cout << "轨迹峰值：速度 " << peak_velocity << " rad/s，加速度 "
                  << peak_acceleration << " rad/s^2" << std::endl;
        if (!confirm(args, "普通系统不保证硬实时；filter_ratio=" + std::to_string(FILTER_RATIO)))
            return 0;

        const auto total_ticks = static_cast<std::size_t>(RUN_TIME * SERVO_RATE);
        const auto period = std::chrono::duration_cast<Clock::duration>(
            std::chrono::duration<double>(1.0 / SERVO_RATE));
        try {
            client->request("request_set_servo_mode", {{"mode", 1}}, false);
            auto next = Clock::now();
            std::size_t count = 0, missed = 0;
            // 相位按 tick 计数推进而非读挂钟：单次发送延迟不会让目标点跳变，
            // 从而不把时序抖动放大成位置台阶。
            for (std::size_t tick = 0; tick <= total_ticks && !interrupted; ++tick) {
                const double elapsed = static_cast<double>(tick) / SERVO_RATE;
                auto target = initial;
                target[CONTROL_JOINT] += AMPLITUDE * amplitude_envelope(elapsed) *
                                         std::sin(2.0 * PI * FREQUENCY * elapsed);
                client->send_oneway("request_servoj",
                                    {{"q", target}, {"filter_ratio", FILTER_RATIO}});
                ++count;
                next += period;
                if (next < Clock::now()) { ++missed; next = Clock::now() + period; }
                std::this_thread::sleep_until(next);
            }
            // 包络已归零，此处再显式回到初始姿态，确保切模式前无残留偏移。
            if (!interrupted) {
                client->send_oneway("request_servoj",
                                    {{"q", initial}, {"filter_ratio", FILTER_RATIO}});
                ++count;
            }
            std::cout << "伺服发送 " << count << " 条，错过周期 " << missed
                      << "；无逐条到位确认" << std::endl;
        } catch (...) { restore_servo(*client); throw; }
        restore_servo(*client);
        std::cout << "ServoJ 示例执行完成" << std::endl;
        return 0;
    });
}
