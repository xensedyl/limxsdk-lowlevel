// 获取逐际二指夹爪完整状态。opening、speed、force 的范围均为 0～100。
// 部分实机固件没有可用的 2F 夹爪数据，会返回机器人原始的 fail_no_data。
#include "robot_utils.hpp"

#include <iostream>

int main(int argc, char** argv) {
    using namespace tron2;
    return guard([&] {
        Options args(argc, argv, connection_values());
        if (args.has("help")) {
            print_connection_help("get_gripper_state [选项]",
                {"opening、speed、force 范围均为 0～100",
                 "固件无夹爪数据时按原样报告 fail_no_data，不改成成功或零状态"});
            return 0;
        }
        require(args.positional.empty(), "状态查询不接受位置参数");
        auto client = connect(args);
        std::cout << "夹爪状态（opening、speed、force 的范围均为 0～100）：" << std::endl;
        print_json(read_gripper_state(*client));
        return 0;
    });
}
