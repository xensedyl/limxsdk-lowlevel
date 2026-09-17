#pragma once
#include "example_common.hpp"
#include <cstdint>
#include <mutex>

// 三个相机示例（camera_stream、camera_stream_3、camera_websocket_benchmark）
// 共用的 Bridge WebSocket 接收、BRDG v1 解析与吞吐统计。
// 图像解码和窗口显示隔离在 Display 后端（camera_display.cpp），
// 因此纯接收的吞吐测试不需要链接 OpenCV。
namespace tron2 {

struct CameraSpec {
    std::string name;
    std::string topic;
};
// 顺序为左、右、顶部；camera_stream 默认取最后一路（顶部）。
extern const std::vector<CameraSpec> CAMERA_SPECS;

// 图像显示后端。纯接收（吞吐测试或 --no-gui）时传 nullptr。
// 实现见 camera_display.cpp，只有需要显示的示例才链接它和 OpenCV。
class Display {
public:
    virtual ~Display() = default;
    virtual void add_window(const std::string& name) = 0;
    // 解码并显示一帧；解码失败返回 false，由调用方计数。
    virtual bool show(const std::string& name, const char* data, std::size_t size) = 0;
    // 处理窗口事件；按 q/Esc 或关闭任一窗口时返回 true。
    virtual bool quit_requested() = 0;
};
std::unique_ptr<Display> make_opencv_display();

// 相机示例共用的参数名和开关名。
const std::set<std::string>& camera_values();
const std::set<std::string>& camera_flags();
void camera_help(const std::string& name);

// 建立连接并运行接收循环。
// benchmark 为 true 时启用预热、要求 --duration > 0、允许缓存并保存压缩帧。
// display 为 nullptr 表示只接收统计，不解码、不显示。
int run_camera_session(const std::string& name, const Options& options, bool benchmark,
                       Display* display);

} // namespace tron2
