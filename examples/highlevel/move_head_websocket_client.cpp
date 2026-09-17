// 交互式原始 MoveH 演示：输入 moveh，然后输入 pitch yaw time。
//
// 用法：
//     move_head_websocket_client
//
// 与 move_head_websocket 的实现完全相同（Python 侧这两个脚本也只有用法行
// 不同），交互循环共用 robot_utils.cpp 的 run_interactive_moveh。
#include "robot_utils.hpp"

#include <iostream>

int main(int argc, char** argv) {
    using namespace tron2;
    return guard([&] {
        Options args(argc, argv, connection_values());
        if (args.has("help")) {
            print_connection_help("move_head_websocket_client [选项]",
                {"输入 moveh 后按提示输入 pitch yaw time（rad rad 秒），输入 exit 退出",
                 "pitch ∈ [-0.78,1.04]，yaw ∈ [-1.57,1.57]，time > 0"});
            return 0;
        }
        require(args.positional.empty(), "此示例不接受位置参数");
        auto client = connect(args);
        std::cout << "机器人已连接：" << client->accid() << std::endl;
        run_interactive_moveh(*client, args);
        return 0;
    });
}
