// OpenCV 图像解码与窗口显示。只有 camera_stream 和 camera_stream_3 链接本文件，
// 吞吐测试 camera_websocket_benchmark 不需要 OpenCV。
#include "camera_common.hpp"

#include <opencv2/core.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgcodecs.hpp>

namespace tron2 {
namespace {

class OpenCvDisplay : public Display {
public:
    ~OpenCvDisplay() override {
        for (const auto& name : names_) {
            try { cv::destroyWindow(name); } catch (...) {}
        }
    }
    void add_window(const std::string& name) override {
        cv::namedWindow(name, cv::WINDOW_NORMAL);
        names_.push_back(name);
    }
    bool show(const std::string& name, const char* data, std::size_t size) override {
        cv::Mat image;
        try {
            cv::Mat encoded(1, static_cast<int>(size), CV_8UC1,
                            const_cast<void*>(static_cast<const void*>(data)));
            image = cv::imdecode(encoded, cv::IMREAD_COLOR);
        } catch (const cv::Exception&) { /* Count invalid image payloads below. */ }
        if (image.empty()) return false;
        cv::imshow(name, image);
        return true;
    }
    bool quit_requested() override {
        const int key = cv::waitKey(1) & 0xff;
        if (key == 'q' || key == 27) return true;
        for (const auto& name : names_)
            if (cv::getWindowProperty(name, cv::WND_PROP_VISIBLE) < 1) return true;
        return false;
    }
private:
    std::vector<std::string> names_;
};

} // namespace

std::unique_ptr<Display> make_opencv_display() { return std::make_unique<OpenCvDisplay>(); }

} // namespace tron2
