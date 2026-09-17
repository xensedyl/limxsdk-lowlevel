// 获取移动版双臂 TRON2 的底盘状态。默认持续打印，与 Python 示例一致。
#include "mobile_platform_common.hpp"

namespace {
constexpr double DEFAULT_INTERVAL = 0.5;  // 秒；0 为单次查询
}

int main(int argc, char** argv) {
    using namespace tron2;
    return guard([&] {
        auto values = connection_values();
        values.insert("interval");
        Options args(argc, argv, values);
        if (args.has("help")) {
            print_connection_help("get_chassis_state [选项]",
                {"--interval 默认为 0.5 秒；0 为单次",
                 "三元素单位按原始反馈保留，不额外换算"});
            return 0;
        }
        require(args.positional.empty(), "状态查询不接受位置参数");
        const double interval = args.real("interval", DEFAULT_INTERVAL);
        require(interval >= 0, "--interval 不能小于 0");
        auto client = connect(args);
        poll_state(*client, interval, read_chassis_state);
        return 0;
    });
}
