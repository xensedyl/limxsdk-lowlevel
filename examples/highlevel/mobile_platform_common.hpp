#pragma once
#include "example_common.hpp"

// 对应 python3/examples/highlevel/mobile_platform_common.py：移动版 TRON2 的
// 底盘与升降台状态查询、命令参数校验和键盘控制。
namespace tron2 {

// 协议文档 3.6.16 的五种底盘模式；实机是否支持由固件决定。
extern const std::set<std::string> CHASSIS_MODES;

// 三元素按原始反馈保留，不额外换算单位。
Json read_chassis_state(Client& client);

// 升降台 q/v 按原始反馈保留。
Json read_lifter_state(Client& client);

// 先打印完整响应体，再返回 data。内层 timestamp 保留整数或浮点原值，
// 外层另外命名为 response_timestamp，不假定两者单位或时钟相同。
Json read_lifter_position(Client& client);

// 状态示例共用的轮询循环。interval 为 0 时只查询一次并返回。
void poll_state(Client& client, double interval, const std::function<Json(Client&)>& read);

// move_chassis 的 SDL2 键盘控制循环。需要桌面环境，点击窗口取得焦点。
int run_keyboard(Client& client, const Options& options);

} // namespace tron2
