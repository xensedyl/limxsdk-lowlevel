// 获取 TRON2 双臂末端位姿。位置单位 m，四元数顺序 wxyz。
#include "robot_utils.hpp"

#include <iostream>

int main(int argc, char** argv) {
    using namespace tron2;
    return guard([&] {
        Options args(argc, argv, connection_values());
        if (args.has("help")) {
            print_connection_help("get_ee_pose [选项]",
                {"位姿顺序为 xyz+wxyz，位置单位 m"});
            return 0;
        }
        require(args.positional.empty(), "状态查询不接受位置参数");
        auto client = connect(args);
        std::cout << "双臂末端位姿（位置单位：m，四元数顺序：wxyz）：" << std::endl;
        print_json(read_ee_pose(*client));
        return 0;
    });
}
