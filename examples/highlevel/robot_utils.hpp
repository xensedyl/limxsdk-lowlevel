#pragma once
#include "example_common.hpp"

// 对应 python3/examples/highlevel/robot_utils.py：双臂、头部和夹爪的状态查询
// 与运动封装。底盘和升降台在 mobile_platform_common.hpp。
namespace tron2 {

// 头部角度限位（rad）。
constexpr double HEAD_PITCH_MIN = -0.78, HEAD_PITCH_MAX = 1.04;
constexpr double HEAD_YAW_MIN = -1.57, HEAD_YAW_MAX = 1.57;
// 左臂末端 z 限位（m）。
constexpr double LEFT_Z_MIN = -0.673, LEFT_Z_MAX = 0.5;

// 18 维状态：左臂 7、左夹爪开口比例、右臂 7、右夹爪开口比例、头部 pitch/yaw。
// 关节和夹爪是两次独立查询，时间戳分别保留。部分固件没有可用的 2F 夹爪数据，
// 会对第二次查询返回 fail_no_data；此时仍返回 16 个有效关节值，两个夹爪占位
// 为 -0.01，并标记 gripper_available=false、附带 gripper_error 原文。
Json read_joint_state(Client& client);

// 双臂末端位姿，位置单位 m，四元数顺序 wxyz。
Json read_ee_pose(Client& client);

// 夹爪完整状态，opening/speed/force 范围均为 0～100。
Json read_gripper_state(Client& client);

// 从 read_ee_pose 的结果取一侧的 7 维 xyz+wxyz。side 为 "left" 或 "right"。
std::vector<double> pose(const Json& state, const std::string& side);

// 请求退出伺服模式。失败只告警，不抛出，避免掩盖触发它的原始异常。
void restore_servo(Client& client) noexcept;

// MoveJ/MoveH 发送后轮询关节反馈，直到欧氏误差小于 tolerance 或超时。
// head 为 true 时比较 q[14..15]（头部），否则比较 q[0..13]（双臂）。
// 到位返回 true；超时抛出；被中断返回 false。
bool wait_until_reached(Client& client, const std::vector<double>& target, bool head,
                        double timeout, double tolerance = 0.05);

// move_head_websocket 与 move_head_websocket_client 共用的交互式 MoveH 循环。
// 两个示例在 Python 侧除用法行外完全相同，这里共用实现、只有用法名不同。
void run_interactive_moveh(Client& client, const Options& options);

} // namespace tron2
