// MoveJ：双臂 14 维关节空间插值运动。
// 要改目标或时间，修改下面的常量后重新编译（与 Python 示例一致）。
#include "robot_utils.hpp"

#include <iostream>

namespace {
constexpr double MOVE_TIME = 5.0;  // 秒
constexpr double TOLERANCE = 0.05; // rad，到位判据

// 顺序为左臂 7 关节、右臂 7 关节，单位为 rad。
const std::vector<double> TARGET_JOINTS{
    0.026899, 0.2612, -0.02709991, -1.5477003, 0.265, 0.0180999, -0.0614999,
    0.008999, -0.269, 0.02069998, -1.5567001, -0.254, -0.02309972, 0.06469989};
}

int main(int argc, char** argv) {
    using namespace tron2;
    return guard([&] {
        Options args(argc, argv, connection_values());
        if (args.has("help")) {
            print_connection_help("move_joint [选项]",
                {"目标关节和运动时间是源码顶部的常量，修改后重新编译",
                 "顺序为左臂 7 关节、右臂 7 关节，单位 rad"});
            return 0;
        }
        require(args.positional.empty(), "此示例不接受位置参数");
        auto client = connect(args);
        std::cout << "机器人已连接：" << client->accid() << std::endl;
        std::cout << "MoveJ 目标关节(rad)：" << Json(TARGET_JOINTS).dump() << std::endl;
        if (!confirm(args, "MoveJ time=" + std::to_string(MOVE_TIME) + " 秒")) return 0;

        client->request("request_set_servo_mode", {{"mode", 0}}, false);
        print_json(client->request("request_movej",
                                   {{"joint", TARGET_JOINTS}, {"time", MOVE_TIME}}));
        if (wait_until_reached(*client, TARGET_JOINTS, false, MOVE_TIME + 5.0, TOLERANCE))
            std::cout << "MoveJ 执行完成（已收到到位反馈）" << std::endl;
        return 0;
    });
}
