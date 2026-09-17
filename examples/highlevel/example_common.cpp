#include "example_common.hpp"
#include <cmath>
#include <iostream>
#include <limits>

namespace tron2 {

const std::vector<std::string> JOINT_STATE_NAMES{
    "abad_L_Joint", "hip_L_Joint", "yaw_L_Joint", "knee_L_Joint",
    "wrist_yaw_L_Joint", "wrist_pitch_L_Joint", "wrist_roll_L_Joint",
    "left_gripper",
    "abad_R_Joint", "hip_R_Joint", "yaw_R_Joint", "knee_R_Joint",
    "wrist_yaw_R_Joint", "wrist_pitch_R_Joint", "wrist_roll_R_Joint",
    "right_gripper",
    "head_pitch_Joint", "head_yaw_Joint"};

double parse_real(const std::string& value, const std::string& name) {
    try {
        std::size_t consumed = 0;
        double result = std::stod(value, &consumed);
        if (consumed != value.size() || !std::isfinite(result)) throw std::invalid_argument("value");
        return result;
    } catch (const std::exception&) { throw std::runtime_error(name + " 必须是有限数值：" + value); }
}

int parse_integer(const std::string& value, const std::string& name) {
    try {
        std::size_t consumed = 0;
        long long result = std::stoll(value, &consumed);
        if (consumed != value.size() || result < std::numeric_limits<int>::min() ||
            result > std::numeric_limits<int>::max()) throw std::invalid_argument("value");
        return static_cast<int>(result);
    } catch (const std::exception&) { throw std::runtime_error(name + " 必须是整数：" + value); }
}

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

Options::Options(int argc, char** argv, std::set<std::string> values, std::set<std::string> flags) {
    bool literal = false;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (!literal && arg == "--") { literal = true; continue; }
        if (!literal && arg == "-h") arg = "--help";
        if (!literal && arg.rfind("--", 0) == 0) {
            auto equals = arg.find('=');
            auto key = arg.substr(2, equals == std::string::npos ? std::string::npos : equals - 2);
            if (flags.count(key)) {
                require(equals == std::string::npos, "开关参数不接受值：" + arg);
                values_[key] = "1";
            } else {
                require(values.count(key), "未知参数：" + arg);
                if (equals != std::string::npos) values_[key] = arg.substr(equals + 1);
                else { require(i + 1 < argc, "缺少参数值：" + arg); values_[key] = argv[++i]; }
                require(!values_[key].empty(), "参数不能为空：" + key);
            }
        } else positional.push_back(arg);
    }
}

bool Options::has(const std::string& name) const { return values_.count(name); }

std::string Options::text(const std::string& name, const std::string& fallback) const {
    auto it = values_.find(name);
    return it == values_.end() ? fallback : it->second;
}

double Options::real(const std::string& name, double fallback) const {
    return has(name) ? parse_real(text(name), name) : fallback;
}

int Options::integer(const std::string& name, int fallback) const {
    return has(name) ? parse_integer(text(name), name) : fallback;
}

bool confirm(const Options& options, const std::string& summary) {
    std::cout << summary << std::endl;
    if (interrupted) return false;
    if (options.has("yes")) return true;
    std::cout << "确认运动范围安全后输入 yes，其他输入取消：" << std::flush;
    std::string answer;
    if (!std::getline(std::cin, answer) || interrupted || answer != "yes") {
        std::cout << "已取消，未发送运动命令" << std::endl;
        return false;
    }
    return true;
}

std::unique_ptr<Client> connect(const Options& options) {
    int port = options.integer("port", 5000);
    double timeout = options.real("timeout", 5);
    require(port > 0 && port <= 65535, "--port 必须为 1～65535");
    require(timeout > 0 && timeout <= 3600, "--timeout 必须大于 0 且不超过 3600 秒");
    auto host = options.text("host", "10.192.1.2");
    auto accid = options.text("accid");
    require(!host.empty(), "--host 不能为空");
    if (options.has("accid"))
        require(accid.find_first_not_of(" \t\r\n") != std::string::npos, "--accid 不能为空白");
    return std::make_unique<Client>(host, port, timeout, accid, options.has("debug"));
}

void print_json(const Json& value) { std::cout << value.dump(2) << std::endl; }

const std::set<std::string>& connection_values() {
    static const std::set<std::string> values{"host", "port", "timeout", "accid"};
    return values;
}

void print_connection_help(const std::string& usage, const std::vector<std::string>& extra) {
    std::cout << "用法：" << usage << "\n"
              << "连接默认：--host 10.192.1.2 --port 5000 --timeout 5；--accid 可选\n"
              << "--help 查看帮助；--debug 打印协议；--yes 跳过运动确认\n";
    for (const auto& line : extra) std::cout << line << "\n";
}

int guard(const std::function<int()>& body) {
    install_signals();
    try {
        return body();
    } catch (const std::exception& error) {
        std::cerr << (interrupted ? "已中断：" : "失败：") << error.what() << std::endl;
        return interrupted ? 130 : 1;
    }
}

} // namespace tron2
