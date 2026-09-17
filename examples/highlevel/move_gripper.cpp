// 逐际二指夹爪控制。命令成功后查询开口反馈，最多等 5 秒。
#include "robot_utils.hpp"

#include <cmath>
#include <iostream>

namespace {
// 取值范围均为 0～100，无量纲。
// opening：开口度，0 对应最小闭合，100 对应张开到最大。
// speed：夹爪速度，数值越大速度越快。
// force：夹持力，数值越大力越大。
constexpr int LEFT_OPENING  = 0;
constexpr int LEFT_SPEED    = 100;
constexpr int LEFT_FORCE    = 1;
constexpr int RIGHT_OPENING = 0;
constexpr int RIGHT_SPEED   = 100;
constexpr int RIGHT_FORCE   = 1;

constexpr int TOLERANCE = 2;         // 开口到位判据
constexpr double WAIT_TIMEOUT = 5.0; // 秒

constexpr bool in_range(int value) { return value >= 0 && value <= 100; }
static_assert(in_range(LEFT_OPENING) && in_range(LEFT_SPEED) && in_range(LEFT_FORCE) &&
              in_range(RIGHT_OPENING) && in_range(RIGHT_SPEED) && in_range(RIGHT_FORCE),
              "夹爪六个参数必须在 0～100");
} // namespace

int main(int argc, char** argv) {
    using namespace tron2;
    return guard([&] {
        Options args(argc, argv, connection_values());
        if (args.has("help")) {
            print_connection_help("move_gripper [选项]",
                {"六个参数是源码顶部的常量，范围均为 0～100，修改后重新编译",
                 "命令成功后查询开口反馈，最多等 5 秒"});
            return 0;
        }
        require(args.positional.empty(), "此示例不接受位置参数");
        const Json target{{"left_opening", LEFT_OPENING}, {"left_speed", LEFT_SPEED},
                          {"left_force", LEFT_FORCE}, {"right_opening", RIGHT_OPENING},
                          {"right_speed", RIGHT_SPEED}, {"right_force", RIGHT_FORCE}};

        auto client = connect(args);
        std::cout << "机器人已连接：" << client->accid() << std::endl;
        std::cout << "左夹爪 opening=" << LEFT_OPENING << ", speed=" << LEFT_SPEED
                  << ", force=" << LEFT_FORCE << std::endl;
        std::cout << "右夹爪 opening=" << RIGHT_OPENING << ", speed=" << RIGHT_SPEED
                  << ", force=" << RIGHT_FORCE << std::endl;
        if (!confirm(args, "确认夹爪内无人员或异物")) return 0;

        print_json(client->request("request_set_limx_2fclaw_cmd", target));

        const auto end = Clock::now() + std::chrono::duration<double>(WAIT_TIMEOUT);
        Json last_state;
        while (!interrupted && Clock::now() < end) {
            last_state = read_gripper_state(*client);
            const bool left_reached =
                std::abs(number(last_state.at("left_opening"), "left_opening") - LEFT_OPENING) <= TOLERANCE;
            const bool right_reached =
                std::abs(number(last_state.at("right_opening"), "right_opening") - RIGHT_OPENING) <= TOLERANCE;
            if (left_reached && right_reached) {
                std::cout << "夹爪已到达目标开口度：" << std::endl;
                print_json(last_state);
                return 0;
            }
            sleep_interruptible(0.1);
        }
        if (interrupted) return 0;
        throw std::runtime_error("夹爪等待到位超时，最后状态：" + last_state.dump());
    });
}
