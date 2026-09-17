// MoveP：双臂末端笛卡尔空间插值运动。
// 相对查询到的当前左臂末端把 z 抬高 LEFT_Z_OFFSET，右臂保持当前位姿。
// 等待时间结束不表示已经到位。
#include "robot_utils.hpp"

#include <iostream>

namespace {
constexpr double MOVE_TIME = 3.0;      // 秒
constexpr double LEFT_Z_OFFSET = 0.02; // m，左臂末端沿基座坐标系 z 轴移动 2 cm
}

int main(int argc, char** argv) {
    using namespace tron2;
    return guard([&] {
        Options args(argc, argv, connection_values());
        if (args.has("help")) {
            print_connection_help("move_pose [选项]",
                {"z 偏移和运动时间是源码顶部的常量，修改后重新编译",
                 "位姿顺序为 xyz+wxyz，位置单位 m；左臂 z 限位 [-0.673, 0.5]",
                 "运动等待时间结束不表示已经到位"});
            return 0;
        }
        require(args.positional.empty(), "此示例不接受位置参数");
        auto client = connect(args);
        const auto state = read_ee_pose(*client);
        auto left = pose(state, "left");
        const auto right = pose(state, "right");

        const double target_z = left[2] + LEFT_Z_OFFSET;
        require(target_z >= LEFT_Z_MIN && target_z <= LEFT_Z_MAX,
                "左臂目标 z=" + std::to_string(target_z) + " m 超出范围");
        left[2] = target_z;

        std::cout << "机器人已连接：" << client->accid() << std::endl;
        std::cout << "左臂目标位姿 xyz+wxyz：" << Json(left).dump() << std::endl;
        std::cout << "右臂保持当前位姿：" << Json(right).dump() << std::endl;

        auto target = left;
        target.insert(target.end(), right.begin(), right.end());
        if (!confirm(args, "MoveP time=" + std::to_string(MOVE_TIME) + " 秒")) return 0;

        client->request("request_set_servo_mode", {{"mode", 0}}, false);
        print_json(client->request("request_movep", {{"pos", target}, {"time", MOVE_TIME}}));
        sleep_interruptible(MOVE_TIME + 0.5);
        std::cout << "动作等待时间结束；未据此确认到位" << std::endl;
        return 0;
    });
}
