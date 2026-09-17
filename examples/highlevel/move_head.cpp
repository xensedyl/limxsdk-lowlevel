// MoveH：头部 pitch、yaw 插值运动。
// 要改目标或时间，修改下面的常量后重新编译（与 Python 示例一致）。
#include "robot_utils.hpp"

#include <iostream>

namespace {
constexpr double MOVE_TIME = 3.0;  // 秒
constexpr double TOLERANCE = 0.05; // rad，到位判据

// [pitch, yaw]，单位为 rad。
// pitch 范围 [-0.78, 1.04]，yaw 范围 [-1.57, 1.57]。
const std::vector<double> TARGET_HEAD{0.0, 0.0};
}

int main(int argc, char** argv) {
    using namespace tron2;
    return guard([&] {
        Options args(argc, argv, connection_values());
        if (args.has("help")) {
            print_connection_help("move_head [选项]",
                {"目标角度和运动时间是源码顶部的常量，修改后重新编译",
                 "顺序为 [pitch, yaw]，单位 rad；pitch ∈ [-0.78,1.04]，yaw ∈ [-1.57,1.57]"});
            return 0;
        }
        require(args.positional.empty(), "此示例不接受位置参数");
        require(TARGET_HEAD[0] >= HEAD_PITCH_MIN && TARGET_HEAD[0] <= HEAD_PITCH_MAX &&
                TARGET_HEAD[1] >= HEAD_YAW_MIN && TARGET_HEAD[1] <= HEAD_YAW_MAX,
                "头部目标角度超出范围");
        auto client = connect(args);
        std::cout << "机器人已连接：" << client->accid() << std::endl;
        std::cout << "MoveH 目标关节(rad)：" << Json(TARGET_HEAD).dump() << std::endl;
        if (!confirm(args, "MoveH time=" + std::to_string(MOVE_TIME) + " 秒")) return 0;

        client->request("request_set_servo_mode", {{"mode", 0}}, false);
        print_json(client->request("request_moveh",
                                   {{"joint", TARGET_HEAD}, {"time", MOVE_TIME}}));
        if (wait_until_reached(*client, TARGET_HEAD, true, MOVE_TIME + 5.0, TOLERANCE))
            std::cout << "MoveH 执行完成（已收到到位反馈）" << std::endl;
        return 0;
    });
}
