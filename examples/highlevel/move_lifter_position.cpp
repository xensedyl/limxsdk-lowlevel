// 升降台绝对位置运动。位置单位 mm，到达时间单位 ms（整数，允许 0 表示最快到达）。
#include "mobile_platform_common.hpp"

int main(int argc, char** argv) {
    using namespace tron2;
    return guard([&] {
        Options args(argc, argv, connection_values());
        if (args.has("help")) {
            print_connection_help("move_lifter_position position_mm duration_ms [选项]",
                {"position 为目标绝对位置，单位 mm",
                 "duration 为整数，单位 ms，允许 0（最快到达）"});
            return 0;
        }
        require(args.positional.size() == 2, "需要两个位置参数：目标值 duration_ms");
        const double position = parse_real(args.positional[0], "目标值");
        const int duration = parse_integer(args.positional[1], "duration");
        require(duration >= 0, "duration 超出允许范围");
        const Json data{{"position", position}, {"duration", duration}};
        if (!confirm(args, "request_set_lifter_position：" + data.dump())) return 0;
        auto client = connect(args);
        print_json(client->request("request_set_lifter_position", data));
        return 0;
    });
}
