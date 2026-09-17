// ServoP：双臂末端位姿连续伺服。左臂 z 做正弦运动，右臂保持当前位姿。
// 高频请求不逐条等待响应，结束时请求退出伺服模式。
//
// 这里的默认轨迹峰值速度约 0.063 m/s，远低于关节能力，因此不需要
// move_servo_joint.cpp 那样的渐变包络。改大 AMPLITUDE 或 FREQUENCY 时
// 请重新核算峰值速度和加速度。
#include "robot_utils.hpp"

#include <cmath>
#include <iostream>
#include <thread>

namespace {
constexpr double PI = 3.14159265358979323846;

constexpr double SERVO_RATE = 1000.0; // Hz
constexpr double RUN_TIME   = 10.0;   // 秒
constexpr double AMPLITUDE  = 0.02;   // m
constexpr double FREQUENCY  = 0.5;    // Hz → 峰值速度约 0.063 m/s
constexpr double SERVOP_TIME = 5.0;   // 每条 servop 指令携带的 time 字段

static_assert(SERVO_RATE >= 1.0 && SERVO_RATE <= 2000.0, "SERVO_RATE 应为 1～2000 Hz");
static_assert(RUN_TIME > 0.0 && AMPLITUDE >= 0.0 && FREQUENCY > 0.0, "伺服参数无效");
} // namespace

int main(int argc, char** argv) {
    using namespace tron2;
    return guard([&] {
        Options args(argc, argv, connection_values());
        if (args.has("help")) {
            print_connection_help("move_servo_pose [选项]",
                {"频率、幅值和时长是源码顶部的常量，修改后重新编译",
                 "左臂 z 限位 [-0.673, 0.5]；轨迹整体必须落在范围内",
                 "普通 Linux 不保证硬实时；无逐条到位确认"});
            return 0;
        }
        require(args.positional.empty(), "此示例不接受位置参数");
        auto client = connect(args);
        const auto state = read_ee_pose(*client);
        const auto initial_left = pose(state, "left");
        const auto initial_right = pose(state, "right");

        require(initial_left[2] - AMPLITUDE >= LEFT_Z_MIN &&
                initial_left[2] + AMPLITUDE <= LEFT_Z_MAX,
                "当前左臂 z 位置过于接近限位，不能执行示例轨迹");

        std::cout << "机器人已连接：" << client->accid() << std::endl;
        std::cout << "ServoP 将以 " << SERVO_RATE << " Hz 运行 " << RUN_TIME << " 秒" << std::endl;
        std::cout << "轨迹峰值速度约 " << AMPLITUDE * 2.0 * PI * FREQUENCY << " m/s" << std::endl;
        if (!confirm(args, "普通系统不保证硬实时")) return 0;

        const auto period = std::chrono::duration_cast<Clock::duration>(
            std::chrono::duration<double>(1.0 / SERVO_RATE));
        try {
            client->request("request_set_servop_mode", Json::object(), false);
            const auto start = Clock::now();
            auto next = start;
            std::size_t count = 0, missed = 0;
            while (!interrupted) {
                const double elapsed = std::chrono::duration<double>(Clock::now() - start).count();
                if (elapsed >= RUN_TIME) break;
                auto target = initial_left;
                target[2] += AMPLITUDE * std::sin(2.0 * PI * FREQUENCY * elapsed);
                client->send_oneway("request_servop", {{"left_pos", target},
                                                       {"right_pos", initial_right},
                                                       {"time", SERVOP_TIME}});
                ++count;
                next += period;
                if (next < Clock::now()) { ++missed; next = Clock::now() + period; }
                std::this_thread::sleep_until(next);
            }
            std::cout << "伺服发送 " << count << " 条，错过周期 " << missed
                      << "；无逐条到位确认" << std::endl;
        } catch (...) { restore_servo(*client); throw; }
        restore_servo(*client);
        std::cout << "ServoP 示例执行完成" << std::endl;
        return 0;
    });
}
