#include "protocol.hpp"

#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <condition_variable>
#include <iostream>
#include <limits>
#include <mutex>
#include <thread>

namespace {
using tron2::Json;
int checks = 0;

void check(bool condition, const std::string& description) {
    ++checks;
    if (!condition) throw std::runtime_error("检查失败：" + description);
}

template<class Function>
void throws(Function function, const std::string& description,
            const std::string& expected_text = "") {
    ++checks;
    try { function(); }
    catch (const std::exception& error) {
        if (!expected_text.empty() && std::string(error.what()).find(expected_text) == std::string::npos)
            throw std::runtime_error(description + "：错误信息不包含 " + expected_text + "；实际：" + error.what());
        return;
    }
    throw std::runtime_error("未抛出预期异常：" + description);
}

void numeric_tests() {
    check(tron2::number(17, "value") == 17, "接受整数");
    check(tron2::number(-0.125, "value") == -0.125, "接受负浮点数");
    check(tron2::number(1478343.282537, "timestamp") == 1478343.282537,
          "保留机器人浮点 timestamp");
    for (const auto& invalid : {Json(), Json(true), Json("1"), Json::object(), Json::array(),
                               Json(std::numeric_limits<double>::infinity()),
                               Json(-std::numeric_limits<double>::infinity()),
                               Json(std::numeric_limits<double>::quiet_NaN())})
        throws([&] { tron2::number(invalid, "value"); }, "拒绝非有限数值/非数字", "value");
    check(tron2::numbers(Json::array({1, -2.5, 0}), 3, "q") == std::vector<double>({1, -2.5, 0}),
          "解析定长数值数组");
    throws([] { tron2::numbers(Json::array({1, 2}), 3, "q"); }, "拒绝数组长度错误");
    throws([] { tron2::numbers(Json::object(), 0, "q"); }, "拒绝非数组");
    throws([] { tron2::numbers(Json::array({1, true}), 2, "q"); }, "拒绝数组内 bool");
}

void json_tests() {
    const Json payload{{"result", "success"}, {"q", Json::array({0.12})}};
    check(tron2::checked_payload({{"data", payload}}) == payload, "返回完整 payload");
    throws([] { tron2::checked_payload({{"data", {{"result", "fail_no_data"}}}}); },
           "保留服务器 fail_no_data", "fail_no_data");
    for (const auto& invalid : {Json(), Json::array(), Json::object(), Json{{"data", nullptr}},
                               Json{{"data", Json::array()}}, Json{{"data", "bad"}}})
        throws([&] { tron2::checked_payload(invalid); }, "拒绝非法响应 data");
    throws([] { tron2::checked_payload({{"data", Json::object()}}); }, "默认必须有 success");
    check(tron2::checked_payload({{"data", {{"q", 1}}}}, false) == Json{{"q", 1}},
          "兼容无 result 的状态响应");
    throws([] { tron2::checked_payload({{"data", {{"result", "fail_no_data"}}}}, false); },
           "可选 result 仍拒绝明确失败", "fail_no_data");
    throws([] { tron2::checked_payload({{"data", {{"result", 1}}}}, false); }, "拒绝非法 result 类型");

    const auto before = tron2::timestamp_ms();
    const auto message = tron2::envelope("TEST_ROBOT", "request_chassis_state", Json::object(), "fixed-guid");
    const auto after = tron2::timestamp_ms();
    check(message.size() == 5 && message["accid"] == "TEST_ROBOT" &&
          message["title"] == "request_chassis_state" && message["guid"] == "fixed-guid" &&
          message["data"] == Json::object(), "协议 envelope 字段");
    check(message["timestamp"].is_number_integer() && message["timestamp"] >= before &&
          message["timestamp"] <= after, "envelope 使用当前毫秒整数时间戳");
    const auto first = tron2::envelope("TEST", "request_test", Json::object());
    const auto second = tron2::envelope("TEST", "request_test", Json::object());
    check(first["guid"].is_string() && first["guid"].get<std::string>().size() == 36 &&
          first["guid"] != second["guid"], "每次请求有独立 UUID");
}

std::string frame(const std::string& mime, const std::string& image = "image-bytes") {
    std::string bytes("BRDG");
    bytes += char(1);
    bytes += static_cast<char>(mime.size());
    bytes += mime;
    bytes.append(8, '\0');
    bytes += image;
    return bytes;
}

void camera_protocol_tests() {
    const auto bytes = frame("image/jpeg");
    const auto offset = tron2::image_offset(bytes);
    check(offset && *offset == 24 && bytes.substr(*offset) == "image-bytes", "BRDG v1 跳过 MIME 和 timestamp");
    check(tron2::image_offset(frame("", "x")) == std::optional<std::size_t>(14), "空 MIME header");
    check(tron2::image_offset(frame(std::string(200, 'm'))) == std::optional<std::size_t>(214),
          "MIME 长度按无符号字节解析");
    check(!tron2::image_offset(frame("image/jpeg", "")), "拒绝空图像载荷");
    for (std::size_t size = 0; size < 24; ++size)
        check(!tron2::image_offset(bytes.substr(0, size)), "拒绝截断 BRDG header");
    auto invalid = bytes;
    invalid[0] = 'X';
    check(!tron2::image_offset(invalid), "拒绝错误 magic");
    invalid = bytes;
    invalid[4] = 2;
    check(!tron2::image_offset(invalid), "拒绝不支持的 BRDG version");
    check(tron2::percent_encode("azAZ09-_.~") == "azAZ09-_.~", "保留 URL unreserved 字符");
    check(tron2::percent_encode("/camera/left image?x=1&y=%") == "%2Fcamera%2Fleft%20image%3Fx%3D1%26y%3D%25",
          "正确编码 topic 和 URL 分隔符");
    check(tron2::percent_encode("\xE4\xB8\xAD") == "%E4%B8%AD", "逐字节编码 UTF-8");
}

void keyboard_tests() {
    tron2::KeyboardControl control;
    tron2::Keys keys;
    keys.forward = true;
    check(control.update(keys, true, 0.2, 0.1) == tron2::zero && !control.armed(),
          "启动时已按住按键不运动");
    keys = {};
    check(control.update(keys, true, 0.2, 0.1) == tron2::zero && control.armed(), "首次松开所有键解锁");
    keys.forward = keys.side_left = keys.left = true;
    check(control.update(keys, true, 0.2, 0.1) == tron2::Command{{0.2, 0.2, 0.1}}, "组合前进横移转向");
    keys.backward = keys.side_right = keys.right = true;
    check(control.update(keys, true, 0.2, 0.1) == tron2::zero, "相反方向互相抵消");
    keys = {};
    keys.backward = keys.side_right = keys.right = true;
    check(control.update(keys, true, 0.2, 0.1) == tron2::Command{{-0.2, -0.2, -0.1}}, "反方向映射");
    keys = {};
    check(control.update(keys, true, 0.2, 0.1) == tron2::zero, "松开立即归零");
    keys.forward = true;
    check(control.update(keys, false, 0.2, 0.1) == tron2::zero && !control.armed(), "失焦停止并锁止");
    check(control.update(keys, true, 0.2, 0.1) == tron2::zero && !control.armed(), "重获焦点仍按键时不恢复运动");
    keys = {};
    control.update(keys, true, 0.2, 0.1);
    keys.forward = true;
    check(control.update(keys, true, 0.2, 0.1) == tron2::Command{{0.2, 0, 0}}, "失焦后松键重新解锁");
    keys.stop = true;
    check(control.update(keys, true, 0.2, 0.1) == tron2::zero && !control.armed(), "空格优先停止并锁止");
    keys.stop = false;
    check(control.update(keys, true, 0.2, 0.1) == tron2::zero, "只松空格不恢复仍按住的运动键");
    keys = {};
    control.update(keys, true, 0.2, 0.1);
    check(control.armed(), "空格停止后需全部松键");
}

// Explicitly opt in with --network. This server only binds the loopback interface
// on an OS-assigned port; these tests never contact a physical robot.
class MockServer {
    using Server = websocketpp::server<websocketpp::config::asio>;
    Server server_;
    std::thread thread_;
    mutable std::mutex received_mutex_;
    std::vector<std::string> received_;
    std::atomic<int> closed_{0};
public:
    explicit MockServer(bool announce = true, std::string announced_accid = "OFFLINE_TEST") {
        server_.clear_access_channels(websocketpp::log::alevel::all);
        server_.clear_error_channels(websocketpp::log::elevel::all);
        server_.init_asio();
        server_.set_open_handler([this, announce, announced_accid](websocketpp::connection_hdl handle) {
            if (announce)
                server_.send(handle, tron2::envelope(announced_accid, "notify_robot_info", {{"motor", "OK"}}).dump(),
                             websocketpp::frame::opcode::text);
        });
        server_.set_close_handler([this](websocketpp::connection_hdl) { ++closed_; });
        server_.set_message_handler([this](websocketpp::connection_hdl handle, Server::message_ptr message) {
            const auto request = Json::parse(message->get_payload());
            const auto title = request.at("title").get<std::string>();
            {
                std::lock_guard<std::mutex> lock(received_mutex_);
                received_.push_back(title);
            }
            auto response = tron2::envelope("OFFLINE_TEST", "response_" + title.substr(8),
                {{"result", "success"}, {"echo", request.at("data")}}, request.at("guid"));
            if (title == "request_wrong_accid") response["accid"] = "ANOTHER_ROBOT";
            if (title == "request_wrong_title") response["title"] = "response_unrelated";
            if (title == "request_wrong_guid") response["guid"] = "another-guid";
            if (title == "request_failure") response["data"] = {{"result", "fail_no_data"}};
            if (title == "request_reports") {
                server_.send(handle, tron2::envelope("OFFLINE_TEST", "notify_robot_info", {{"marker", "expected"}}).dump(),
                             websocketpp::frame::opcode::text);
                server_.send(handle, tron2::envelope("ANOTHER_ROBOT", "notify_robot_info", {{"marker", "wrong"}}).dump(),
                             websocketpp::frame::opcode::text);
            }
            if (title != "request_silent") server_.send(handle, response.dump(), websocketpp::frame::opcode::text);
        });
        server_.listen(boost::asio::ip::tcp::endpoint(boost::asio::ip::address_v4::loopback(), 0));
        server_.start_accept();
        thread_ = std::thread([this] { server_.run(); });
    }
    ~MockServer() {
        websocketpp::lib::error_code error;
        server_.stop_listening(error);
        server_.stop();
        if (thread_.joinable()) thread_.join();
    }
    int port() {
        boost::system::error_code error;
        const auto endpoint = server_.get_local_endpoint(error);
        if (error) throw std::runtime_error(error.message());
        return endpoint.port();
    }
    std::size_t count(const std::string& title) const {
        std::lock_guard<std::mutex> lock(received_mutex_);
        return std::count(received_.begin(), received_.end(), title);
    }
    int closed_connections() const { return closed_.load(); }
};

void client_tests() {
    MockServer server;
    tron2::Client client("127.0.0.1", server.port(), 1.0);
    check(client.accid() == "OFFLINE_TEST", "从 notify 自动发现 ACCID");
    const auto first = client.send("request_first", {{"value", 1}});
    const auto second = client.send("request_second", {{"value", 2}});
    check(client.wait(second)["data"]["echo"]["value"] == 2, "并发请求匹配 second");
    check(client.wait(first)["data"]["echo"]["value"] == 1, "并发请求匹配 first");
    throws([&] { client.request("request_failure"); }, "服务端失败返回原始错误", "fail_no_data");
    for (const auto* title : {"request_wrong_accid", "request_wrong_title", "request_wrong_guid", "request_silent"}) {
        const auto ticket = client.send(title);
        throws([&] { client.wait(ticket, 0.06); }, std::string("忽略不匹配响应并超时：") + title, "超时");
    }
    check(client.request("request_after_timeout")["data"]["result"] == "success", "超时后仍可继续查询");
}

void cancelled_send_test() {
    MockServer server;
    std::mutex mutex;
    std::condition_variable changed;
    bool entered = false, released = false;
    tron2::WebSocket socket("ws://127.0.0.1:" + std::to_string(server.port()), 1.0,
        [&](const std::string&, bool) {
            std::unique_lock<std::mutex> lock(mutex);
            if (entered) return;
            entered = true;
            changed.notify_all();
            changed.wait(lock, [&] { return released; });
        });
    // Release the deliberately blocked receiver before socket destruction,
    // including if a check fails, so a failure cannot hang the test runner.
    struct Release {
        std::mutex& mutex;
        std::condition_variable& changed;
        bool& released;
        void now() {
            std::lock_guard<std::mutex> lock(mutex);
            released = true;
            changed.notify_all();
        }
        ~Release() { now(); }
    } release{mutex, changed, released};
    {
        std::unique_lock<std::mutex> lock(mutex);
        check(changed.wait_for(lock, std::chrono::seconds(1), [&] { return entered; }),
              "mock 已阻塞接收线程");
    }
    throws([&] { socket.send(tron2::envelope("OFFLINE_TEST", "request_expired", Json::object()).dump()); },
           "阻塞 IO 时发送超时", "超时");
    release.now();
    socket.send(tron2::envelope("OFFLINE_TEST", "request_after_stall", Json::object()).dump());
    const auto deadline = tron2::Clock::now() + std::chrono::seconds(1);
    while (server.count("request_after_stall") == 0 && tron2::Clock::now() < deadline)
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    check(server.count("request_after_stall") == 1, "IO 恢复后可发送新指令");
    check(server.count("request_expired") == 0, "超时指令不在 IO 恢复后补发");
}

void discovery_regression_tests() {
    {
        MockServer server(true, "ANOTHER_ROBOT");
        tron2::Client client("127.0.0.1", server.port(), 1.0, "OFFLINE_TEST");
        // A reply on the same ordered WebSocket stream is a barrier: the
        // initial foreign report has been handled before this request returns.
        client.request("request_barrier");
        check(client.accid() == "OFFLINE_TEST", "显式 ACCID 不被其他机器人上报覆盖");
        check(client.robot_info().is_null(), "显式 ACCID 不缓存其他机器人的首条上报");
        client.request("request_reports");
        const auto report = client.robot_info();
        check(report["accid"] == "OFFLINE_TEST" && report["data"]["marker"] == "expected",
              "匹配上报可缓存，随后其他机器人的上报不能覆盖");
    }
    {
        MockServer server(false);
        const auto start = tron2::Clock::now();
        throws([&] { tron2::Client client("127.0.0.1", server.port(), 0.08); },
               "无自动发现上报时构造失败", "ACCID");
        check(tron2::Clock::now() - start < std::chrono::seconds(2), "发现超时后清理没有挂住");
        const auto deadline = tron2::Clock::now() + std::chrono::seconds(1);
        while (server.closed_connections() == 0 && tron2::Clock::now() < deadline)
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        check(server.closed_connections() == 1, "构造失败时关闭 WebSocket 连接");
        tron2::Client explicit_client("127.0.0.1", server.port(), 1.0, "OFFLINE_TEST");
        check(explicit_client.request("request_after_failed_discovery")["data"]["result"] == "success",
              "无上报服务端仍可通过显式 ACCID 正常查询");
    }
}
} // namespace

int main(int argc, char** argv) {
    try {
        if (argc > 2 || (argc == 2 && std::string(argv[1]) != "--network"))
            throw std::runtime_error("用法：tron2_highlevel_tests [--network]");
        numeric_tests();
        json_tests();
        camera_protocol_tests();
        keyboard_tests();
        if (argc == 2) { client_tests(); cancelled_send_test(); discovery_regression_tests(); }
        std::cout << "通过 " << checks << " 项检查" << (argc == 2 ? "（包含本机 WebSocket mock）" : "（纯离线）") << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
