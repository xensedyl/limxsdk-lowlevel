// 键盘控制底盘运动。需要有桌面的 SDL2 窗口，点击窗口取得焦点。
// 速度值是协议范围 [-1,1] 内的值，不标为 m/s。
#include "mobile_platform_common.hpp"

#include <string>

int main(int argc, char** argv) {
    using namespace tron2;
    return guard([&] {
        auto values = connection_values();
        values.insert({"speed", "turn-speed", "mode", "rate", "response-timeout"});
        Options args(argc, argv, values);
        if (args.has("help")) {
            print_connection_help("move_chassis [选项]",
                {"--speed 默认 0.5，--turn-speed 默认 0.5，取值范围 (0,1]",
                 "--rate 指令刷新频率 Hz，默认 20，范围 [1,100]",
                 "--response-timeout 默认 0.5 秒",
                 "--mode 可选，进入键盘控制前先设置底盘模式",
                 "W/S 前后，A/D 转向，Q/E 发送 y（支持情况取决于固件），松开/失焦/空格停止"});
            return 0;
        }
        require(args.positional.empty(), "此示例使用 --选项，不接受位置参数");
        const double speed = args.real("speed", 0.5), turn = args.real("turn-speed", 0.5);
        const double rate = args.real("rate", 20), timeout = args.real("response-timeout", 0.5);
        require(speed > 0 && speed <= 1 && turn > 0 && turn <= 1 && rate >= 1 && rate <= 100 && timeout > 0,
                "速度应为 (0,1]，rate 为 [1,100]，response-timeout > 0");
        require(!args.has("mode") || CHASSIS_MODES.count(args.text("mode")), "无效底盘模式");
        if (!confirm(args, "键盘控制底盘；速度值=" + std::to_string(speed) +
                     "，模式=" + args.text("mode", "保持当前（未知）"))) return 0;
        auto client = connect(args);
        if (args.has("mode"))
            print_json(client->request("request_set_chassis_mode", {{"move_mode", args.text("mode")}}));
        return run_keyboard(*client, args);
    });
}
