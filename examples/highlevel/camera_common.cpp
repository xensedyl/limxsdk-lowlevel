#include "camera_common.hpp"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <thread>

namespace tron2 {

const std::vector<CameraSpec> CAMERA_SPECS{
    {"left", "/camera/left/color/image_resized/compressed"},
    {"right", "/camera/right/color/image_resized/compressed"},
    {"top", "/camera/top/color/image_raw/compressed"}};

namespace {

Clock::time_point after(Clock::time_point start, double seconds) {
    return start + std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(seconds));
}
double elapsed(Clock::time_point end, Clock::time_point start) {
    return std::chrono::duration<double>(end - start).count();
}

struct Counts {
    std::uint64_t frames = 0;
    std::uint64_t bytes = 0;
    std::uint64_t other = 0;
};
struct CameraState {
    std::mutex mutex;
    Counts counts;
    std::string latest;
    std::uint64_t sequence = 0;
    Clock::time_point last_image = Clock::now();
    Clock::time_point measure_start = Clock::time_point::max();
    Clock::time_point measure_end = Clock::time_point::max();
    std::vector<std::string> saved;
    std::size_t saved_bytes = 0;
    std::string error;
};
// States outlive their socket callbacks, including construction/cleanup failures.
struct Stream {
    CameraSpec spec;
    std::shared_ptr<CameraState> state = std::make_shared<CameraState>();
    std::unique_ptr<WebSocket> socket;
    std::uint64_t shown_sequence = 0;
    Counts previous;
    std::uint64_t decode_errors = 0;
};

void receive_image(const std::shared_ptr<CameraState>& state, const std::string& packet,
                   bool binary, bool keep_latest, std::size_t save_limit) {
    const auto now = Clock::now();
    const auto offset = binary ? image_offset(packet) : std::optional<std::size_t>{};
    std::lock_guard<std::mutex> lock(state->mutex);
    const bool measuring = now >= state->measure_start && now < state->measure_end;
    if (!offset) {
        if (measuring) ++state->counts.other;
        return;
    }
    state->last_image = now;
    if (keep_latest) {
        state->latest = packet;
        ++state->sequence;
    }
    if (!measuring) return;
    ++state->counts.frames;
    state->counts.bytes += packet.size();
    if (save_limit && state->error.empty()) {
        if (packet.size() > save_limit - state->saved_bytes) {
            state->error = "压缩帧缓存达到 --max-save-mb 限制，停止测试；已缓存的帧仍可保存";
        } else {
            state->saved.push_back(packet);
            state->saved_bytes += packet.size();
        }
    }
}

Counts snapshot(Stream& stream) {
    std::lock_guard<std::mutex> lock(stream.state->mutex);
    return stream.state->counts;
}

void check_stream(Stream& stream, double frame_timeout) {
    stream.socket->check();
    std::lock_guard<std::mutex> lock(stream.state->mutex);
    if (!stream.state->error.empty()) throw std::runtime_error(stream.spec.name + ": " + stream.state->error);
    if (frame_timeout > 0 && elapsed(Clock::now(), stream.state->last_image) > frame_timeout)
        throw std::runtime_error(stream.spec.name + " 超过 " + std::to_string(frame_timeout) + " 秒未收到有效 BRDG 图像");
}

void print_rate(const std::string& name, const Counts& counts, double seconds) {
    const double fps = seconds > 0 ? counts.frames / seconds : 0;
    const double mbps = seconds > 0 ? counts.bytes * 8.0 / seconds / 1000000.0 : 0;
    std::cout << name << ": " << std::fixed << std::setprecision(1) << fps << " FPS, "
              << mbps << " Mbit/s, " << counts.frames << " 帧";
}

void report(std::vector<Stream>& streams, double seconds, bool final) {
    for (auto& stream : streams) {
        auto counts = snapshot(stream);
        Counts delta{counts.frames - stream.previous.frames, counts.bytes - stream.previous.bytes,
                     counts.other - stream.previous.other};
        print_rate(stream.spec.name, final ? counts : delta, seconds);
        std::cout << "，非图像消息=" << (final ? counts.other : delta.other);
        if (stream.decode_errors) std::cout << "，累计解码失败=" << stream.decode_errors;
        std::cout << std::endl;
        stream.previous = counts;
    }
}

void save_frames(const std::vector<Stream>& streams, const std::string& directory) {
    // A fresh run directory avoids overwriting an earlier recording.
    const auto root = std::filesystem::path(directory) / ("run-" + std::to_string(timestamp_ms()) + "-" + uuid());
    std::filesystem::create_directories(root);
    for (const auto& stream : streams) {
        const auto path = root / stream.spec.name;
        std::filesystem::create_directories(path);
        std::size_t index = 0;
        for (const auto& packet : stream.state->saved) {
            const auto offset = image_offset(packet);
            if (!offset) continue;
            const auto mime = packet.substr(6, static_cast<unsigned char>(packet[5]));
            const auto ext = mime == "image/jpeg" || mime == "image/jpg" ? ".jpg" :
                             mime == "image/png" ? ".png" : ".bin";
            std::ostringstream filename;
            filename << std::setfill('0') << std::setw(6) << index++ << ext;
            std::ofstream output(path / filename.str(), std::ios::binary);
            if (!output) throw std::runtime_error("无法写入相机文件：" + (path / filename.str()).string());
            output.write(packet.data() + *offset, static_cast<std::streamsize>(packet.size() - *offset));
            output.close();
            if (!output) throw std::runtime_error("保存相机文件失败：" + (path / filename.str()).string());
        }
        std::cout << stream.spec.name << " 已保存 " << index << " 帧：" << path << std::endl;
    }
}

} // namespace

const std::set<std::string>& camera_values() {
    static const std::set<std::string> values{
        "host", "scheme", "port", "topic", "fps", "duration", "warmup", "report-interval",
        "connect-timeout", "frame-timeout", "save-dir", "max-save-mb"};
    return values;
}

const std::set<std::string>& camera_flags() {
    static const std::set<std::string> flags{"help", "insecure", "no-gui"};
    return flags;
}

void camera_help(const std::string& name) {
    std::cout << "用法：" << name << " [选项]\n"
              << "camera_stream：单路（默认顶部）；camera_stream_3：左/右/顶部三路。\n"
              << "camera_websocket_benchmark：三路纯接收测试，不解码、不显示。\n"
              << "  --host IP                Bridge 地址，默认 10.192.1.4\n"
              << "  --scheme ws|wss          默认 wss\n"
              << "  --port PORT              默认使用 ws/wss 标准端口\n"
              << "  --insecure               显式允许自签名 TLS 证书\n"
              << "  --fps N                  服务端最大放行帧率（显示默认 60，测试 100）\n"
              << "  --topic TOPIC            仅 camera_stream：压缩图像话题\n"
              << "  --connect-timeout SEC    全部连接建立超时，默认 15 秒\n"
              << "  --frame-timeout SEC      无有效图像超时，默认 15 秒，0 禁用\n"
              << "  --report-interval SEC    打印间隔，默认 1 秒\n"
              << "  --duration SEC           显示默认 0（持续），测试默认 20 秒\n"
              << "  --no-gui                 显示命令只统计接收，不解码或创建窗口\n"
              << "  --warmup SEC             仅测试：预热时长，默认 3 秒\n"
              << "  --save-dir DIR           仅测试：缓存压缩帧，测试结束后保存\n"
              << "  --max-save-mb N          仅测试：三路合计缓存上限，默认 512 MiB\n"
              << "窗口按 q/Esc、关闭窗口或终端 Ctrl+C 退出。TLS 默认校验证书。\n";
}

int run_camera_session(const std::string& name, const Options& options, bool benchmark,
                       Display* display) {
    require(options.positional.empty(), "相机命令不接受位置参数");
    require(name == "camera_stream" || !options.has("topic"), "--topic 仅适用于 camera_stream");
    require(benchmark || (!options.has("warmup") && !options.has("save-dir") && !options.has("max-save-mb")),
            "--warmup、--save-dir、--max-save-mb 仅适用于 camera_websocket_benchmark");
    const auto host = options.text("host", "10.192.1.4");
    const auto scheme = options.text("scheme", "wss");
    require(!host.empty() && host.find_first_of("/ ?#\t\r\n") == std::string::npos,
            "--host 需要主机名或 IP；协议使用 --scheme，端口使用 --port");
    require(scheme == "ws" || scheme == "wss", "--scheme 必须是 ws 或 wss");
    const int port = options.integer("port", scheme == "wss" ? 443 : 80);
    const int fps = options.integer("fps", benchmark ? 100 : 60);
    const double connect_timeout = options.real("connect-timeout", 15);
    const double frame_timeout = options.real("frame-timeout", 15);
    const double duration = options.real("duration", benchmark ? 20 : 0);
    const double warmup = benchmark ? options.real("warmup", 3) : 0;
    const double interval = options.real("report-interval", 1);
    require(port > 0 && port <= 65535, "--port 必须为 1～65535");
    require(fps > 0, "--fps 必须大于 0");
    require(connect_timeout > 0 && connect_timeout <= 3600, "--connect-timeout 必须大于 0 且不超过 3600 秒");
    require(frame_timeout >= 0 && frame_timeout <= 3600, "--frame-timeout 必须为 0～3600 秒");
    require(duration >= 0 && duration <= 604800 && (!benchmark || duration > 0),
            "--duration 显示时允许 0，测试时必须大于 0，且不超过 604800 秒");
    require(warmup >= 0 && warmup <= 3600, "--warmup 必须为 0～3600 秒");
    require(interval > 0 && interval <= 3600, "--report-interval 必须大于 0 且不超过 3600 秒");
    const int max_save_mb = options.integer("max-save-mb", 512);
    require(max_save_mb > 0 && max_save_mb <= 16384, "--max-save-mb 必须为 1～16384 MiB");

    const bool gui = display != nullptr && !options.has("no-gui");
    std::vector<CameraSpec> specs = CAMERA_SPECS;
    if (name == "camera_stream") specs = {{"top", options.text("topic", CAMERA_SPECS.back().topic)}};
    for (const auto& spec : specs)
        require(!spec.topic.empty() && spec.topic.front() == '/', "相机话题必须以 / 开头");
    const std::size_t save_limit = options.has("save-dir") ?
        static_cast<std::size_t>(max_save_mb) * 1024 * 1024 / specs.size() : 0;

    if (gui) {
#if defined(__linux__)
        require(std::getenv("DISPLAY") || std::getenv("WAYLAND_DISPLAY"),
                "未检测到桌面显示环境；请使用 --no-gui 或 camera_websocket_benchmark");
#endif
        for (const auto& spec : specs) display->add_window("TRON2 " + spec.name);
    }
    std::vector<Stream> streams;
    streams.reserve(specs.size());
    const auto connect_deadline = after(Clock::now(), connect_timeout);
    for (const auto& spec : specs) {
        Stream stream;
        stream.spec = spec;
        const auto url = scheme + "://" + host + ":" + std::to_string(port) +
            "/bridge/ws?topic=" + percent_encode(spec.topic) + "&kind=image&max_fps=" + std::to_string(fps);
        std::cout << spec.name << " 正在连接：" << url << std::endl;
        const auto remaining = elapsed(connect_deadline, Clock::now());
        require(remaining > 0, "建立全部相机连接超时");
        stream.socket = std::make_unique<WebSocket>(url, remaining,
            [state = stream.state, gui, save_limit](const std::string& packet, bool binary) {
                receive_image(state, packet, binary, gui, save_limit);
            }, options.has("insecure"));
        std::cout << spec.name << " 已连接" << std::endl;
        streams.push_back(std::move(stream));
    }

    const auto ready = Clock::now();
    const auto measure_start = after(ready, warmup);
    const auto measure_end = duration > 0 ? after(measure_start, duration) : Clock::time_point::max();
    for (auto& stream : streams) {
        std::lock_guard<std::mutex> lock(stream.state->mutex);
        stream.state->measure_start = measure_start;
        stream.state->measure_end = measure_end;
        // Display rates start once all connections are ready.
        if (!benchmark) stream.state->counts = {};
    }
    if (warmup > 0) std::cout << "三路已连接，预热 " << warmup << " 秒" << std::endl;
    std::cout << (gui ? "视频窗口按 q/Esc 退出；终端 Ctrl+C 退出"
                      : "只接收并统计 BRDG 图像，不解码、不显示；Ctrl+C 退出") << std::endl;
    auto previous_report = measure_start;
    auto actual_end = ready;
    std::string failure;
    try {
        bool done = false;
        while (!interrupted && !done) {
            auto now = Clock::now();
            if (now >= measure_end) break;
            for (auto& stream : streams) {
                check_stream(stream, frame_timeout);
                if (!gui) continue;
                std::string packet;
                {
                    std::lock_guard<std::mutex> lock(stream.state->mutex);
                    if (stream.shown_sequence != stream.state->sequence) {
                        packet = stream.state->latest;
                        stream.shown_sequence = stream.state->sequence;
                    }
                }
                if (!packet.empty()) {
                    const auto offset = *image_offset(packet);
                    if (!display->show("TRON2 " + stream.spec.name, packet.data() + offset,
                                       packet.size() - offset))
                        ++stream.decode_errors;
                }
            }
            if (gui && display->quit_requested()) done = true;
            now = std::min(Clock::now(), measure_end);
            if (now >= measure_start && elapsed(now, previous_report) >= interval) {
                report(streams, elapsed(now, previous_report), false);
                previous_report = now;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(gui ? 2 : 10));
        }
        actual_end = std::min(Clock::now(), measure_end);
    } catch (const std::exception& error) {
        actual_end = std::min(Clock::now(), measure_end);
        failure = error.what();
    }
    // Freeze the window before joining receiver threads; callbacks can still arrive during close.
    for (auto& stream : streams) {
        std::lock_guard<std::mutex> lock(stream.state->mutex);
        stream.state->measure_end = actual_end;
    }
    for (auto& stream : streams) stream.socket->close();
    for (auto& stream : streams)
        if (failure.empty() && !stream.state->error.empty())
            failure = stream.spec.name + ": " + stream.state->error;
    std::cout << "最终接收统计（" << std::max(0.0, elapsed(actual_end, measure_start)) << " 秒）：" << std::endl;
    report(streams, std::max(0.0, elapsed(actual_end, measure_start)), true);
    if (options.has("save-dir")) save_frames(streams, options.text("save-dir"));
    if (!failure.empty()) throw std::runtime_error(failure);
    return interrupted ? 130 : 0;
}

} // namespace tron2
