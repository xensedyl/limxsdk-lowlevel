#pragma once
#include "protocol.hpp"
#include <functional>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace tron2 {

// 18 维关节状态的名称，与 python3/examples/highlevel/example_common.py 的
// JOINT_STATE_NAMES 一致：左臂 7、左夹爪、右臂 7、右夹爪、头部 2。
extern const std::vector<std::string> JOINT_STATE_NAMES;

// 18 维状态里两个夹爪的下标。
constexpr std::size_t LEFT_GRIPPER_INDEX = 7;
constexpr std::size_t RIGHT_GRIPPER_INDEX = 15;

class Options {
public:
    Options(int argc, char** argv, std::set<std::string> values,
            std::set<std::string> flags = {"help", "yes", "debug"});
    bool has(const std::string& name) const;
    std::string text(const std::string& name, const std::string& fallback = "") const;
    double real(const std::string& name, double fallback) const;
    int integer(const std::string& name, int fallback) const;
    std::vector<std::string> positional;
private:
    std::map<std::string, std::string> values_;
};

double parse_real(const std::string& text, const std::string& name);
int parse_integer(const std::string& text, const std::string& name);
void require(bool condition, const std::string& message);
bool confirm(const Options& options, const std::string& summary);
std::unique_ptr<Client> connect(const Options& options);
void print_json(const Json& value);

// 所有示例共用的连接参数名，构造 Options 时作为起点。
const std::set<std::string>& connection_values();

// 打印 --help 的公共部分。extra 里每个元素单独一行，用于补充本示例特有的说明。
void print_connection_help(const std::string& usage,
                           const std::vector<std::string>& extra = {});

// main() 的统一包装：安装信号处理、捕获异常并映射退出码。
// 正常返回 body 的返回值；异常返回 1；被 Ctrl+C 中断返回 130。
int guard(const std::function<int()>& body);

} // namespace tron2
