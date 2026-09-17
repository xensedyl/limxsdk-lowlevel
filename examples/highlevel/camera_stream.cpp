// 单路相机视频流，默认顶部相机。使用独立 Bridge WebSocket（默认 10.192.1.4:443），
// 解析 BRDG v1 图像消息，OpenCV 负责解码和窗口显示，只取最新帧。
#include "camera_common.hpp"

int main(int argc, char** argv) {
    using namespace tron2;
    return guard([&] {
        Options args(argc, argv, camera_values(), camera_flags());
        if (args.has("help")) { camera_help("camera_stream"); return 0; }
        auto display = make_opencv_display();
        return run_camera_session("camera_stream", args, false, display.get());
    });
}
