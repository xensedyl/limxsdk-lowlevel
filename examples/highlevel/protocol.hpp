#pragma once

#include <nlohmann/json.hpp>
#include <array>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace tron2 {
using Json = nlohmann::json;
using Clock = std::chrono::steady_clock;
inline volatile std::sig_atomic_t interrupted = 0;
void install_signals();
void sleep_interruptible(double seconds);
std::string uuid();
std::int64_t timestamp_ms();
double number(const Json& value, const std::string& field);
std::vector<double> numbers(const Json& value, std::size_t size, const std::string& field);
Json envelope(const std::string& accid, const std::string& title, const Json& data,
              const std::string& guid = uuid());
Json checked_payload(const Json& response, bool require_result = true);

// Binary camera frames never enter the control client's response queue.
class WebSocket {
public:
    using Handler = std::function<void(const std::string&, bool)>;
    WebSocket(const std::string& url, double timeout, Handler handler, bool insecure = false);
    ~WebSocket();
    WebSocket(const WebSocket&) = delete;
    WebSocket& operator=(const WebSocket&) = delete;
    // Synchronous enqueue with a 500 ms scheduling deadline. Call from the
    // owner thread, not Handler; timed-out queued messages are cancelled.
    void send(const std::string& message);
    void check() const;
    // The owning thread must close/destroy the socket (never inside Handler).
    void close() noexcept;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

struct Pending;
struct Ticket {
    std::string guid;
    Clock::time_point sent_at;
    std::shared_ptr<Pending> pending;
};

// One receiver owns the socket; requests match title + GUID + ACCID.
class Client {
public:
    Client(std::string host, int port, double timeout, std::string accid = "", bool debug = false);
    ~Client();
    Client(const Client&) = delete;
    Client& operator=(const Client&) = delete;
    Ticket send(const std::string& title, const Json& data = Json::object(),
                bool require_result = true);
    Json wait(const Ticket& ticket, double timeout = -1, bool ignore_interrupt = false);
    std::optional<Json> poll(const Ticket& ticket);
    Json request(const std::string& title, const Json& data = Json::object(),
                 bool require_result = true);
    void send_oneway(const std::string& title, const Json& data);
    void check() const;
    Json robot_info() const;
    const std::string& accid() const { return accid_; }
private:
    void receive(const std::string& message, bool binary);
    struct State;
    std::unique_ptr<State> state_;
    std::string accid_;
    double timeout_;
    bool debug_;
    // Last member: stop/join callbacks before any other member is destroyed,
    // including when discovery throws from the constructor.
    std::unique_ptr<WebSocket> socket_;
};

using Command = std::array<double, 3>;
inline constexpr Command zero{{0, 0, 0}};
struct Keys {
    bool forward = false, backward = false, left = false, right = false;
    bool side_left = false, side_right = false, stop = false;
    bool motion() const { return forward || backward || left || right || side_left || side_right; }
};
class KeyboardControl {
public:
    Command update(const Keys& keys, bool focused, double speed, double turn);
    bool armed() const { return armed_; }
private:
    bool armed_ = false;
};

// BRDG v1: magic(4), version(1), MIME length(1), MIME, uint64 timestamp, image.
std::optional<std::size_t> image_offset(const std::string& bytes);
std::string percent_encode(const std::string& text);
} // namespace tron2
