#include "robot_utils.hpp"
#include <cmath>
#include <iostream>
#include <sstream>

namespace tron2 {

Json read_joint_state(Client& client) {
    auto response = client.request("request_get_joint_state", Json::object(), false);
    auto q = numbers(response.at("data").at("q"), 16, "q");
    Json gripper_response;
    bool gripper_available = true;
    std::string gripper_error;
    try {
        gripper_response = client.request("request_get_limx_2fclaw_state", Json::object(), false);
    } catch (const std::exception& error) {
        // Some TRON2 firmware builds do not expose the optional 2F claw state
        // and answer fail_no_data while joint feedback is valid. Keep the
        // usable 16 joint values, matching Python's unavailable gripper
        // sentinel, but do not hide other failures.
        gripper_error = error.what();
        if (gripper_error.find("fail_no_data") == std::string::npos) throw;
        gripper_available = false;
        std::cerr << "夹爪状态不可用，保留关节状态（机器人原始响应）：\n"
                  << gripper_error << std::endl;
    }
    std::vector<double> state(q.begin(), q.begin() + 7);
    state.push_back(gripper_available ?
        number(gripper_response.at("data").at("left_opening"), "left_opening") / 100.0 : -0.01);
    state.insert(state.end(), q.begin() + 7, q.begin() + 14);
    state.push_back(gripper_available ?
        number(gripper_response.at("data").at("right_opening"), "right_opening") / 100.0 : -0.01);
    state.insert(state.end(), q.begin() + 14, q.end());
    Json result{{"timestamp", response.value("timestamp", Json())},
                {"gripper_timestamp", gripper_available ? gripper_response.value("timestamp", Json()) : Json()},
                {"gripper_available", gripper_available}, {"states", state}};
    if (!gripper_available) result["gripper_error"] = gripper_error;
    return result;
}

Json read_ee_pose(Client& client) {
    auto response = client.request("request_get_move_pose", Json::object(), false);
    Json state = response.at("data");
    for (auto side : {"left", "right"}) {
        numbers(state.at(std::string(side) + "_position"), 3, "position");
        numbers(state.at(std::string(side) + "_quat"), 4, "quat");
    }
    state["timestamp"] = response.value("timestamp", Json());
    return state;
}

Json read_gripper_state(Client& client) {
    auto response = client.request("request_get_limx_2fclaw_state", Json::object(), false);
    Json state = response.at("data");
    for (auto key : {"left_opening", "left_speed", "left_force",
                     "right_opening", "right_speed", "right_force"})
        number(state.at(key), key);
    if (!state.contains("timestamp")) state["timestamp"] = response.value("timestamp", Json());
    return state;
}

std::vector<double> pose(const Json& state, const std::string& side) {
    auto result = numbers(state.at(side + "_position"), 3, side + "_position");
    auto quat = numbers(state.at(side + "_quat"), 4, side + "_quat");
    result.insert(result.end(), quat.begin(), quat.end());
    return result;
}

void restore_servo(Client& client) noexcept {
    try {
        auto ticket = client.send("request_set_servo_mode", {{"mode", 0}}, false);
        client.wait(ticket, 0.5, true);
        std::cout << "退出伺服模式请求已获响应" << std::endl;
    } catch (const std::exception& error) {
        std::cerr << "退出伺服模式未获确认：" << error.what()
                  << "；请检查机器人，必要时使用实体急停。" << std::endl;
    }
}

bool wait_until_reached(Client& client, const std::vector<double>& target, bool head,
                        double timeout, double tolerance) {
    const auto end = Clock::now() + std::chrono::duration<double>(timeout);
    while (!interrupted && Clock::now() < end) {
        auto response = client.request("request_get_joint_state", Json::object(), false);
        auto q = numbers(response.at("data").at("q"), 16, "q");
        double error = 0;
        for (std::size_t i = 0; i < target.size(); ++i) {
            double delta = q[i + (head ? 14 : 0)] - target[i];
            error += delta * delta;
        }
        if (std::sqrt(error) < tolerance) return true;
        sleep_interruptible(0.1);
    }
    if (interrupted) return false;
    throw std::runtime_error("等待到位超时；请求成功不等于已到位");
}

void run_interactive_moveh(Client& client, const Options& options) {
    while (!interrupted) {
        std::cout << "输入 moveh 控制头部，exit 退出：" << std::flush;
        std::string line;
        if (!std::getline(std::cin, line) || line == "exit") break;
        if (line != "moveh") continue;
        std::cout << "请输入 pitch yaw time（rad rad 秒）：" << std::flush;
        if (!std::getline(std::cin, line)) break;
        std::istringstream input(line);
        std::string p, y, t, extra;
        if (!(input >> p >> y >> t) || (input >> extra)) { std::cerr << "需要三个数值\n"; continue; }
        double pitch = parse_real(p, "pitch"), yaw = parse_real(y, "yaw"), seconds = parse_real(t, "time");
        require(pitch >= HEAD_PITCH_MIN && pitch <= HEAD_PITCH_MAX &&
                yaw >= HEAD_YAW_MIN && yaw <= HEAD_YAW_MAX && seconds > 0,
                "头部角度或时间超出范围");
        if (!confirm(options, "MoveH：" + line)) continue;
        print_json(client.request("request_moveh", {{"joint", {pitch, yaw}}, {"time", seconds}}));
    }
}

} // namespace tron2
