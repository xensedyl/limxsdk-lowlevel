// 升降台速度运动。速度单位 mm/s（可正可负），持续时间单位 ms（整数，必须 > 0）。
#include "mobile_platform_common.hpp"

int main(int argc, char** argv) {
    using namespace tron2;
    return guard([&] {
        Options args(argc, argv, connection_values());
        if (args.has("help")) {
            print_connection_help("move_lifter_velocity velocity_mm_s duration_ms [选项]",
                {"velocity 单位 mm/s，可正可负",
                 "duration 为整数，单位 ms，必须 > 0"});
            return 0;
        }
        require(args.positional.size() == 2, "需要两个位置参数：目标值 duration_ms");
        const double velocity = parse_real(args.positional[0], "目标值");
        const int duration = parse_integer(args.positional[1], "duration");
        require(duration > 0, "duration 超出允许范围");
        const Json data{{"velocity", velocity}, {"duration", duration}};
        if (!confirm(args, "request_set_lifter_velocity：" + data.dump())) return 0;
        auto client = connect(args);
        print_json(client->request("request_set_lifter_velocity", data));
        return 0;
    });
}
