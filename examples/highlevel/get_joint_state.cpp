// 获取 TRON2 双臂、夹爪和头部关节位置。
// 输出 18 维：左臂 7 关节、左夹爪开口比例、右臂 7 关节、右夹爪开口比例、头部 pitch/yaw。
// 关节和夹爪是两次独立查询，时间戳分别保留。部分固件没有可用的 2F 夹爪数据，
// 这时仍输出有效的 16 个关节值，两个夹爪占位为 -0.01。
#include "robot_utils.hpp"

#include <iomanip>
#include <iostream>

int main(int argc, char** argv) {
    using namespace tron2;
    return guard([&] {
        Options args(argc, argv, connection_values());
        if (args.has("help")) {
            print_connection_help("get_joint_state [选项]",
                {"夹爪不可用时占位 -0.01，并标记 gripper_available=false"});
            return 0;
        }
        require(args.positional.empty(), "状态查询不接受位置参数");
        auto client = connect(args);
        const auto state = read_joint_state(*client);
        const auto values = numbers(state.at("states"), JOINT_STATE_NAMES.size(), "states");

        std::cout << "timestamp: " << state.at("timestamp") << "\n";
        std::cout << "gripper_timestamp: " << state.at("gripper_timestamp")
                  << "  gripper_available: " << std::boolalpha
                  << state.at("gripper_available").get<bool>() << "\n";
        for (std::size_t i = 0; i < values.size(); ++i) {
            const bool gripper = i == LEFT_GRIPPER_INDEX || i == RIGHT_GRIPPER_INDEX;
            std::cout << "[" << std::setfill('0') << std::setw(2) << i << "] "
                      << std::setfill(' ') << std::left << std::setw(24) << JOINT_STATE_NAMES[i]
                      << std::right << std::fixed << std::setprecision(6) << std::showpos
                      << values[i] << std::noshowpos << " " << (gripper ? "ratio" : "rad") << "\n";
        }
        std::cout << std::flush;
        return 0;
    });
}
