#include "protocol.hpp"
#include <websocketpp/client.hpp>
#include <websocketpp/config/asio_client.hpp>
#include <websocketpp/config/asio_no_tls_client.hpp>
#include <boost/asio/ssl/host_name_verification.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <cmath>
#include <condition_variable>
#include <future>
#include <iostream>
#include <map>
#include <mutex>
#include <thread>
#include <type_traits>

namespace tron2 {
namespace {
void on_signal(int) { interrupted = 1; }
std::string string_field(const Json& value, const char* key) {
    auto it = value.find(key);
    return it != value.end() && it->is_string() ? it->get<std::string>() : "";
}

struct SocketBase {
    virtual ~SocketBase() = default;
    virtual void send(const std::string&) = 0;
    virtual void check() const = 0;
    virtual void close() noexcept = 0;
};

template<class Config> class Socket final : public SocketBase {
    using Endpoint = websocketpp::client<Config>;
    Endpoint endpoint_;
    typename Endpoint::connection_ptr connection_;
    mutable std::mutex mutex_;
    std::condition_variable changed_;
    std::thread thread_;
    bool opened_ = false, closed_ = false, closing_ = false;
    std::thread::id io_thread_id_;
    std::string error_;

    void fail(const std::string& error) {
        std::lock_guard<std::mutex> lock(mutex_);
        error_ = error;
        changed_.notify_all();
    }
public:
    Socket(const std::string& url, double timeout, WebSocket::Handler handler, bool insecure) {
        endpoint_.clear_access_channels(websocketpp::log::alevel::all);
        endpoint_.clear_error_channels(websocketpp::log::elevel::all);
        endpoint_.init_asio();
        endpoint_.set_open_handshake_timeout(static_cast<long>(timeout * 1000));
        endpoint_.set_close_handshake_timeout(200);
        endpoint_.set_max_message_size(32 * 1024 * 1024);
        if constexpr (std::is_same_v<Config, websocketpp::config::asio_tls_client>) {
            auto uri = websocketpp::uri(url);
            endpoint_.set_tls_init_handler([insecure, host = uri.get_host()](websocketpp::connection_hdl) {
                auto ctx = std::make_shared<boost::asio::ssl::context>(boost::asio::ssl::context::tls_client);
                if (insecure) {
                    ctx->set_verify_mode(boost::asio::ssl::verify_none);
                } else {
                    ctx->set_default_verify_paths();
                    ctx->set_verify_mode(boost::asio::ssl::verify_peer);
                    ctx->set_verify_callback(boost::asio::ssl::host_name_verification(host));
                }
                return ctx;
            });
        }
        endpoint_.set_open_handler([this](websocketpp::connection_hdl) {
            std::lock_guard<std::mutex> lock(mutex_);
            opened_ = true;
            changed_.notify_all();
        });
        endpoint_.set_fail_handler([this](websocketpp::connection_hdl) {
            fail("WebSocket 连接失败：" + connection_->get_ec().message());
        });
        endpoint_.set_close_handler([this](websocketpp::connection_hdl) {
            std::lock_guard<std::mutex> lock(mutex_);
            closed_ = true;
            changed_.notify_all();
        });
        endpoint_.set_message_handler([this, handler = std::move(handler)](
            websocketpp::connection_hdl, typename Endpoint::message_ptr message) {
            try {
                handler(message->get_payload(), message->get_opcode() == websocketpp::frame::opcode::binary);
            } catch (const std::exception& e) { fail(std::string("接收处理失败：") + e.what()); }
        });
        websocketpp::lib::error_code ec;
        connection_ = endpoint_.get_connection(url, ec);
        if (ec) throw std::runtime_error(ec.message());
        endpoint_.connect(connection_);
        thread_ = std::thread([this] {
            {
                std::lock_guard<std::mutex> lock(mutex_);
                io_thread_id_ = std::this_thread::get_id();
            }
            try { endpoint_.run(); }
            catch (const std::exception& e) { fail(e.what()); }
        });
        try {
            auto deadline = Clock::now() + std::chrono::duration<double>(timeout);
            std::unique_lock<std::mutex> lock(mutex_);
            while (!opened_ && !closed_ && error_.empty() && !interrupted && Clock::now() < deadline)
                changed_.wait_for(lock, std::chrono::milliseconds(20));
            if (interrupted) throw std::runtime_error("连接已取消");
            if (!error_.empty()) throw std::runtime_error(error_);
            if (!opened_ || closed_) throw std::runtime_error("WebSocket 建连超时或连接已关闭");
        } catch (...) { close(); throw; }
    }
    ~Socket() override { close(); }
    void check() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!error_.empty()) throw std::runtime_error(error_);
        if (!opened_ || closed_ || closing_) throw std::runtime_error("WebSocket 未连接或已关闭");
    }
    void send(const std::string& text) override {
        check();
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (std::this_thread::get_id() == io_thread_id_)
                throw std::runtime_error("不能在 WebSocket 接收回调中同步发送");
        }
        struct SendOperation {
            std::mutex mutex;
            std::promise<void> result;
            bool cancelled = false;
            bool completed = false;
            Clock::time_point deadline = Clock::now() + std::chrono::milliseconds(500);
        };
        auto operation = std::make_shared<SendOperation>();
        auto result = operation->result.get_future();
        // WebSocket++ reads its send-buffer size without a lock. Keep both
        // that read and all application sends on the single IO thread.
        endpoint_.get_io_service().post([this, operation, text] {
            std::lock_guard<std::mutex> lock(operation->mutex);
            if (operation->cancelled) return;
            try {
                if (Clock::now() >= operation->deadline)
                    throw std::runtime_error("WebSocket 发送调度超时，已丢弃过期指令");
                check();
                if (connection_->get_buffered_amount() + text.size() > 256 * 1024)
                    throw std::runtime_error("WebSocket 发送积压，停止控制；不能保证停止指令送达");
                websocketpp::lib::error_code ec;
                endpoint_.send(connection_->get_handle(), text, websocketpp::frame::opcode::text, ec);
                if (ec) throw std::runtime_error(ec.message());
                operation->result.set_value();
            } catch (...) {
                operation->result.set_exception(std::current_exception());
            }
            operation->completed = true;
        });
        if (result.wait_until(operation->deadline) != std::future_status::ready) {
            // Serialize cancellation with the enqueue operation: once this
            // returns a timeout, the queued task cannot later send the text.
            std::lock_guard<std::mutex> lock(operation->mutex);
            if (!operation->completed) {
                operation->cancelled = true;
                throw std::runtime_error("WebSocket 发送调度超时，已取消过期指令");
            }
        }
        result.get();
    }
    void close() noexcept override {
        if (!thread_.joinable()) return;
        try {
            {
                std::lock_guard<std::mutex> lock(mutex_);
                closing_ = true;
            }
            endpoint_.get_io_service().post([this] {
                websocketpp::lib::error_code ec;
                endpoint_.close(connection_->get_handle(), websocketpp::close::status::normal, "exit", ec);
                if (ec) {
                    std::lock_guard<std::mutex> lock(mutex_);
                    closed_ = true;
                    changed_.notify_all();
                }
            });
            std::unique_lock<std::mutex> lock(mutex_);
            changed_.wait_for(lock, std::chrono::milliseconds(250), [this] { return closed_; });
        } catch (...) {}
        endpoint_.stop();
        thread_.join();
    }
};
} // namespace

void install_signals() {
    interrupted = 0;
    std::signal(SIGINT, on_signal);
    std::signal(SIGTERM, on_signal);
}
void sleep_interruptible(double seconds) {
    auto end = Clock::now() + std::chrono::duration<double>(seconds);
    while (!interrupted && Clock::now() < end)
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
}
std::string uuid() {
    static thread_local boost::uuids::random_generator gen;
    return boost::uuids::to_string(gen());
}
std::int64_t timestamp_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}
double number(const Json& value, const std::string& field) {
    if (!value.is_number() || !std::isfinite(value.get<double>()))
        throw std::runtime_error(field + " 必须是有限数值，实际为 " + value.dump());
    return value.get<double>();
}
std::vector<double> numbers(const Json& value, std::size_t size, const std::string& field) {
    if (!value.is_array() || value.size() != size)
        throw std::runtime_error(field + " 应包含 " + std::to_string(size) + " 个数值");
    std::vector<double> result;
    for (const auto& item : value) result.push_back(number(item, field));
    return result;
}
Json envelope(const std::string& accid, const std::string& title, const Json& data, const std::string& guid) {
    return {{"accid", accid}, {"title", title}, {"guid", guid}, {"timestamp", timestamp_ms()}, {"data", data}};
}
Json checked_payload(const Json& response, bool require_result) {
    if (!response.contains("data") || !response["data"].is_object())
        throw std::runtime_error("响应 data 不是对象：\n" + response.dump(2));
    const auto& data = response["data"];
    if ((require_result || data.contains("result")) && string_field(data, "result") != "success")
        throw std::runtime_error("机器人报告请求失败：\n" + response.dump(2));
    return data;
}
struct WebSocket::Impl { std::unique_ptr<SocketBase> socket; };
WebSocket::WebSocket(const std::string& url, double timeout, Handler handler, bool insecure)
    : impl_(std::make_unique<Impl>()) {
    if (url.rfind("wss://", 0) == 0)
        impl_->socket = std::make_unique<Socket<websocketpp::config::asio_tls_client>>(url, timeout, std::move(handler), insecure);
    else if (url.rfind("ws://", 0) == 0)
        impl_->socket = std::make_unique<Socket<websocketpp::config::asio_client>>(url, timeout, std::move(handler), false);
    else throw std::runtime_error("WebSocket URL 必须以 ws:// 或 wss:// 开头");
}
WebSocket::~WebSocket() = default;
void WebSocket::send(const std::string& message) { impl_->socket->send(message); }
void WebSocket::check() const { impl_->socket->check(); }
void WebSocket::close() noexcept { if (impl_ && impl_->socket) impl_->socket->close(); }

struct Pending {
    std::string response_title;
    bool require_result;
    std::optional<Json> response;
};
struct Client::State {
    mutable std::mutex mutex;
    std::condition_variable changed;
    std::map<std::string, std::weak_ptr<Pending>> pending;
    std::string discovered_accid;
    Json info;
};
Client::Client(std::string host, int port, double timeout, std::string accid, bool debug)
    : state_(std::make_unique<State>()), accid_(std::move(accid)), timeout_(timeout), debug_(debug) {
    const auto url = "ws://" + host + ":" + std::to_string(port);
    // Initialize before the receiver starts so --accid also filters reports.
    state_->discovered_accid = accid_;
    socket_ = std::make_unique<WebSocket>(url, timeout, [this](const auto& data, bool binary) { receive(data, binary); });
    if (accid_.empty()) {
        const auto deadline = Clock::now() + std::chrono::duration<double>(timeout);
        std::unique_lock<std::mutex> lock(state_->mutex);
        while (state_->discovered_accid.empty() && Clock::now() < deadline && !interrupted) {
            socket_->check();
            state_->changed.wait_for(lock, std::chrono::milliseconds(20));
        }
        if (state_->discovered_accid.empty())
            throw std::runtime_error("等待 notify_robot_info/ACCID 超时或被取消；可使用 --accid");
        accid_ = state_->discovered_accid;
    }
    std::cout << "已连接：" << url << "\n机器人 ACCID：" << accid_ << std::endl;
}
Client::~Client() { if (socket_) socket_->close(); }
void Client::receive(const std::string& text, bool binary) {
    if (binary) return;
    const auto response = Json::parse(text, nullptr, false);
    if (!response.is_object()) return;
    const auto title = string_field(response, "title");
    std::string accid = string_field(response, "accid");
    if (title == "notify_robot_info" && accid.empty() && response.contains("data") && response["data"].is_object())
        accid = string_field(response["data"], "accid");
    std::lock_guard<std::mutex> lock(state_->mutex);
    if (title == "notify_robot_info") {
        if (!accid.empty()) {
            if (state_->discovered_accid.empty()) state_->discovered_accid = accid;
            if (accid == state_->discovered_accid) {
                state_->info = response;
                state_->changed.notify_all();
            }
        }
    }
    auto it = state_->pending.find(string_field(response, "guid"));
    if (it == state_->pending.end()) return;
    auto pending = it->second.lock();
    if (!pending) { state_->pending.erase(it); return; }
    if (title != pending->response_title || accid != state_->discovered_accid) return;
    pending->response = response;
    state_->pending.erase(it);
    state_->changed.notify_all();
}
Ticket Client::send(const std::string& title, const Json& data, bool require_result) {
    if (title.rfind("request_", 0) != 0) throw std::runtime_error("无效请求名");
    check();
    Ticket ticket{uuid(), Clock::now(), std::make_shared<Pending>()};
    ticket.pending->response_title = "response_" + title.substr(8);
    ticket.pending->require_result = require_result;
    {
        std::lock_guard<std::mutex> lock(state_->mutex);
        for (auto it = state_->pending.begin(); it != state_->pending.end();)
            if (it->second.expired()) it = state_->pending.erase(it); else ++it;
        if (state_->pending.size() >= 256) throw std::runtime_error("待回复请求过多");
        state_->pending[ticket.guid] = ticket.pending;
    }
    auto message = envelope(accid_, title, data, ticket.guid);
    if (debug_) std::cout << "[发送] " << message.dump() << std::endl;
    socket_->send(message.dump());
    return ticket;
}
std::optional<Json> Client::poll(const Ticket& ticket) {
    std::optional<Json> response;
    {
        std::lock_guard<std::mutex> lock(state_->mutex);
        response = ticket.pending->response;
    }
    if (response) checked_payload(*response, ticket.pending->require_result);
    return response;
}
Json Client::wait(const Ticket& ticket, double timeout, bool ignore_interrupt) {
    auto deadline = Clock::now() + std::chrono::duration<double>(timeout < 0 ? timeout_ : timeout);
    while (Clock::now() < deadline) {
        if (auto response = poll(ticket)) {
            if (debug_) std::cout << "[响应] " << response->dump() << std::endl;
            return *response;
        }
        if (interrupted && !ignore_interrupt) throw std::runtime_error("请求等待已取消");
        check();
        std::unique_lock<std::mutex> lock(state_->mutex);
        state_->changed.wait_for(lock, std::chrono::milliseconds(5));
    }
    throw std::runtime_error("等待 " + ticket.pending->response_title + " 超时，guid=" + ticket.guid);
}
Json Client::request(const std::string& title, const Json& data, bool require_result) {
    return wait(send(title, data, require_result));
}
void Client::send_oneway(const std::string& title, const Json& data) {
    check();
    socket_->send(envelope(accid_, title, data).dump());
}
void Client::check() const { socket_->check(); }
Json Client::robot_info() const {
    std::lock_guard<std::mutex> lock(state_->mutex);
    return state_->info;
}
Command KeyboardControl::update(const Keys& k, bool focused, double speed, double turn) {
    if (!focused || k.stop) { armed_ = false; return zero; }
    if (!armed_) { if (!k.motion()) armed_ = true; return zero; }
    return {{speed * (int(k.forward) - int(k.backward)),
             speed * (int(k.side_left) - int(k.side_right)),
             turn * (int(k.left) - int(k.right))}};
}
std::optional<std::size_t> image_offset(const std::string& bytes) {
    if (bytes.size() < 14 || bytes.compare(0, 4, "BRDG") != 0 || bytes[4] != 1) return {};
    const auto offset = 14u + static_cast<unsigned char>(bytes[5]);
    if (offset >= bytes.size()) return {};
    return offset;
}
std::string percent_encode(const std::string& text) {
    static const char hex[] = "0123456789ABCDEF";
    std::string out;
    for (unsigned char c : text) {
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
            c == '-' || c == '_' || c == '.' || c == '~') out += char(c);
        else { out += '%'; out += hex[c >> 4]; out += hex[c & 15]; }
    }
    return out;
}
} // namespace tron2
