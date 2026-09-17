// 设置移动版双臂 TRON2 的底盘运动模式（SDK 文档 3.6.16）。
// 程序会传递文档中的五种模式，但实机是否支持由固件决定：
// 已知 DACH_TRON2A_174 仅接受 ackerman，其余四种返回 fail_unsupported_mode。
// 不支持的软件 emergency_stop 不能当作实体急停使用。
#include "mobile_platform_common.hpp"

#include <iostream>

int main(int argc, char** argv) {
    using namespace tron2;
    return guard([&] {
        Options args(argc, argv, connection_values());
        if (args.has("help")) {
            std::string list;
            for (const auto& mode : CHASSIS_MODES) list += (list.empty() ? "" : ",") + mode;
            print_connection_help("set_mode_chassis {" + list + "} [选项]",
                {"实机是否支持某个模式由固件决定，不支持时返回 fail_unsupported_mode"});
            return 0;
        }
        require(args.positional.size() == 1 && CHASSIS_MODES.count(args.positional[0]),
                "需要一个有效的底盘模式");
        const Json data{{"move_mode", args.positional[0]}};
        if (!confirm(args, "request_set_chassis_mode：" + data.dump())) return 0;
        auto client = connect(args);
        print_json(client->request("request_set_chassis_mode", data));
        return 0;
    });
}
