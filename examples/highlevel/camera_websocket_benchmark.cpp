// 三路相机纯接收吞吐测试。无需桌面、不解码图像、不链接 OpenCV。
// 统计的是传输帧率和吞吐，不代表解码显示帧率。
#include "camera_common.hpp"

int main(int argc, char** argv) {
    using namespace tron2;
    return guard([&] {
        Options args(argc, argv, camera_values(), camera_flags());
        if (args.has("help")) { camera_help("camera_websocket_benchmark"); return 0; }
        return run_camera_session("camera_websocket_benchmark", args, true, nullptr);
    });
}
