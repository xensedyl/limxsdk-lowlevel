// 三路相机视频流（左、右、顶部）。每路使用独立连接，显示只取最新帧。
#include "camera_common.hpp"

int main(int argc, char** argv) {
    using namespace tron2;
    return guard([&] {
        Options args(argc, argv, camera_values(), camera_flags());
        if (args.has("help")) { camera_help("camera_stream_3"); return 0; }
        auto display = make_opencv_display();
        return run_camera_session("camera_stream_3", args, false, display.get());
    });
}
