// 获取升降台位置。先打印完整响应体，再打印整理后的 data。
// 内层 timestamp 保留原值，外层另外命名为 response_timestamp。
#include "mobile_platform_common.hpp"

namespace {
constexpr double DEFAULT_INTERVAL = 0.0;  // 秒；0 为单次查询
}

int main(int argc, char** argv) {
    using namespace tron2;
    return guard([&] {
        auto values = connection_values();
        values.insert("interval");
        Options args(argc, argv, values);
        if (args.has("help")) {
            print_connection_help("get_lifter_position [选项]",
                {"--interval 默认为 0 秒；0 为单次",
                 "不假定内层 timestamp 与外层 response_timestamp 的单位或时钟相同"});
            return 0;
        }
        require(args.positional.empty(), "状态查询不接受位置参数");
        const double interval = args.real("interval", DEFAULT_INTERVAL);
        require(interval >= 0, "--interval 不能小于 0");
        auto client = connect(args);
        poll_state(*client, interval, read_lifter_position);
        return 0;
    });
}
