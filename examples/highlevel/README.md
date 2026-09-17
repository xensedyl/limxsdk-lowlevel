# TRON2 C++ 上层接口示例

参考[官方文档 §3.7.1 C++ 示例](https://www.limxdynamics.com/zh/documents/847884267345285120#3.7.1-C%2B%2B-%E7%A4%BA%E4%BE%8B%E5%AE%9E%E7%8E%B0)，使用 WebSocket++、Boost 和 nlohmann/json 实现。无需安装 Python 或链接低层 `limxsdk` 库。

本目录与 `python3/examples/highlevel` 一一对应：**一个示例一个源文件、一个可执行文件**，名称沿用 Python 脚本名（去掉 `.py`）。本目录独立构建。

## 目录结构

共享模块的边界与 Python 侧的模块一致：

| C++ | Python 对应 | 内容 |
|---|---|---|
| `protocol.{hpp,cpp}` | — | 连接、JSON 协议、请求匹配、键盘状态机、BRDG 解析 |
| `example_common.{hpp,cpp}` | `example_common.py` | 参数解析、确认提示、连接构造、关节名表 |
| `robot_utils.{hpp,cpp}` | `robot_utils.py` | 双臂、头部、夹爪的状态查询与运动 |
| `mobile_platform_common.{hpp,cpp}` | `mobile_platform_common.py` | 底盘、升降台、SDL2 键盘控制 |
| `camera_common.{hpp,cpp}` | — | Bridge WebSocket 接收与吞吐统计，不含 OpenCV |
| `camera_display.cpp` | — | OpenCV 解码与窗口显示，只有需要显示的示例链接 |

每个示例只链接自己需要的库：双臂类不链 SDL2 和 OpenCV，`move_chassis` 才链 SDL2，两个 `camera_stream*` 才链 OpenCV。

## 构建

Ubuntu / Debian 依赖（本地在 Ubuntu 22.04 / GCC 10.5 / CMake 3.22 / OpenCV 4.5.4 / SDL2 2.0.20 编译验证；Boost 需 ≥1.73）：

```bash
sudo apt install build-essential cmake libwebsocketpp-dev libboost-system-dev \
    libssl-dev nlohmann-json3-dev libsdl2-dev libopencv-dev
```

在仓库根目录执行：

```bash
cmake -S examples/highlevel -B build/highlevel -DCMAKE_BUILD_TYPE=Release
cmake --build build/highlevel -j2
ctest --test-dir build/highlevel --output-on-failure
```

构建出 21 个可执行文件，直接运行：

```bash
./build/highlevel/get_joint_state
./build/highlevel/move_joint --help
```

所有示例都有 `--help`。共用 `--host`、`--port`、`--timeout`（秒）、`--accid`、`--debug`、`--yes`。控制和状态默认连接 `ws://10.192.1.2:5000`。未指定 ACCID 时从 `notify_robot_info` 获取；请求按 GUID、响应名称及 ACCID 匹配。错误响应会完整打印，`fail_no_data`、`fail_unsupported_mode` 不会改成成功或零状态。

运动示例在执行前要求输入 `yes`；`--yes` 可跳过交互。参数中的目标需要根据自己的机器人检查。程序不会主动切换上层开发者模式。

## 两种参数风格

与 Python 侧一致，示例分两类，**不统一**：

- **双臂、头部、夹爪（11 个）**：可调参数写在源文件顶部的 `constexpr`，改参数需修改源码并重新编译。命令行只接受连接类选项。这样打开 `move_servo_joint.cpp` 就能一眼看到全部可调量，与对应的 Python 脚本对齐。
- **底盘、升降台、相机（10 个）**：保留完整命令行参数，与对应 Python 脚本的 `argparse` 一致。

## 双臂和头部

只读状态：

```bash
./build/highlevel/get_joint_state
./build/highlevel/get_ee_pose
./build/highlevel/get_gripper_state
```

`get_joint_state` 输出 18 维：左臂 7 关节、左夹爪开口比例、右臂 7 关节、右夹爪开口比例、头部 pitch/yaw。关节和夹爪是两次独立查询，时间戳分别保留。部分实机固件没有可用的 2F 夹爪数据，会对第二次查询返回机器人原始的 `fail_no_data`；这时程序仍输出有效的 16 个关节值，并将两个夹爪占位为 `-0.01`，同时标记 `gripper_available=false` 和打印完整错误。`get_gripper_state` 单独查询时仍会报告该机器人错误。`get_ee_pose` 输出双臂位置和四元数，位姿顺序 `xyz+wxyz`。

运动示例：

```bash
./build/highlevel/move_joint         # MoveJ：14 维关节，目标见源码 TARGET_JOINTS
./build/highlevel/move_head          # MoveH：pitch/yaw，见源码 TARGET_HEAD
./build/highlevel/move_pose          # MoveP：左臂 z 抬高 LEFT_Z_OFFSET，右臂保持
./build/highlevel/move_servo_joint   # ServoJ：见源码顶部常量
./build/highlevel/move_servo_pose    # ServoP：左臂 z 正弦运动
./build/highlevel/move_gripper       # 夹爪：六个参数见源码顶部
./build/highlevel/move_head_websocket         # 交互式原始 MoveH
./build/highlevel/move_head_websocket_client  # 同上，实现共用
```

各示例可调常量位于源文件顶部：

| 示例 | 常量 |
|---|---|
| `move_joint.cpp` | `MOVE_TIME`、`TARGET_JOINTS`（14 维，rad） |
| `move_head.cpp` | `MOVE_TIME`、`TARGET_HEAD`（pitch/yaw，rad） |
| `move_pose.cpp` | `MOVE_TIME`、`LEFT_Z_OFFSET`（m） |
| `move_servo_joint.cpp` | `SERVO_RATE`、`RUN_TIME`、`AMPLITUDE`、`FREQUENCY`、`RAMP_TIME`、`FILTER_RATIO`、`CONTROL_JOINT` |
| `move_servo_pose.cpp` | `SERVO_RATE`、`RUN_TIME`、`AMPLITUDE`、`FREQUENCY` |
| `move_gripper.cpp` | `LEFT_OPENING/SPEED/FORCE`、`RIGHT_OPENING/SPEED/FORCE`（均 0～100） |

越界的常量组合由 `static_assert` 在编译期拒绝，不用等到运行时。

两个原始头部示例的实现完全相同（Python 侧这两个脚本也只有用法行不同），交互循环共用 `robot_utils.cpp` 的 `run_interactive_moveh`。MoveJ/MoveH 检查关节到位反馈，MoveP 的等待时间结束不表示已经到位。

### ServoJ 抖动

ServoJ 无插值，下发什么就跟什么，因此**抖动通常来自指令本身超出关节能力**，不是通信问题。下发轨迹的峰值量级为：

```
峰值角速度   = AMPLITUDE * 2*pi*FREQUENCY
峰值角加速度 = AMPLITUDE * (2*pi*FREQUENCY)^2
```

默认 `AMPLITUDE=0.3` rad、`FREQUENCY=0.3` Hz 对应峰值速度约 0.57 rad/s、加速度约 1.07 rad/s²，在关节能跟住的范围内；启动时会打印这两个值，方便改常量时先核算。相比之下 0.3 rad @ 10 Hz 的峰值速度达 18.8 rad/s、加速度 1184 rad/s²，电机必然饱和并激发结构共振。

本示例另有三项抑制抖动的措施：

- **幅度包络**：两端各 `RAMP_TIME` 秒余弦渐变，消除起停瞬间的速度阶跃。
- **tick 相位**：相位按发送计数推进（`elapsed = tick / SERVO_RATE`）而非读挂钟，单次发送延迟不会让目标点跳变成位置台阶。
- **`filter_ratio`**：协议字段，`1.0` 为无滤波，越小机器人侧一阶平滑越强、跟随滞后越大。默认 `0.3`。若降低 `FREQUENCY` 后已经不抖，建议调回 `0.7`～`1.0`，把滤波留给真正需要的高频场景。

`move_servo_pose` 默认轨迹峰值速度仅约 0.063 m/s，没有同样的问题，因此不加包络。改大幅值或频率时请重新核算。

ServoJ/ServoP 高频请求不逐条等待响应，结束时请求退出伺服模式；普通 Linux 线程调度不保证硬实时。

## 底盘

```bash
# 默认每次返回后间隔 0.5 秒持续打印；Ctrl+C 退出
./build/highlevel/get_chassis_state
./build/highlevel/get_chassis_state --interval 0.2
./build/highlevel/get_chassis_state --interval 0

./build/highlevel/set_mode_chassis ackerman
./build/highlevel/move_chassis --mode ackerman --speed 0.2 --turn-speed 0.2
```

键盘控制需要有桌面的 SDL2 窗口，点击窗口取得焦点：

- `W/S` 或上下键发送 `x`；`A/D` 或左右键发送 `yaw`；`Q/E` 发送 `y`。
- 按住时默认 20 Hz 持续发送，可用 `--rate` 调整；松开对应方向立即归零。
- 空格、失焦、最小化触发零速度并锁止；重新取得焦点后需先松开所有方向键才能重新运动。
- Esc、关闭窗口或 Ctrl+C 时尝试发送零速度，等待其成功响应。
- `--response-timeout` 默认 0.5 秒。运动响应失败、超时或断线时尝试发送零速度并退出。
- 窗口标题与终端显示的是目标值，`--debug` 显示发送和响应 JSON。`success` 不是实际运动或停稳证明，网络断开时停止指令可能无法送达。

速度值是协议范围 `[-1,1]` 内的值，不标为 m/s。程序支持传递文档中的 `ackerman/parallel/park/spinning/emergency_stop`，但是否支持由实机固件决定。此前这台 `DACH_TRON2A_174` 仅接受 `ackerman`，其余四种模式均返回 `fail_unsupported_mode`；Q/E 的 `y` 指令没有横移效果。C++ 不会将横移自动替换为转向，也不会将不支持的软件 `emergency_stop` 当作实体急停。

## 升降台

```bash
./build/highlevel/get_lifter_state
./build/highlevel/get_lifter_position --interval 0.5

# 绝对位置 mm、到达时间 ms（整数，允许 0 表示最快到达）
./build/highlevel/move_lifter_position 600 2000

# 速度 mm/s、持续时间 ms（整数，必须 >0）
./build/highlevel/move_lifter_velocity 50 2000
```

`get_lifter_position` 打印完整响应，内层 `timestamp` 保留整数或浮点原值，外层另外命名为 `response_timestamp`，不假定两者的单位或时钟相同。升降台 `q/v` 和底盘状态三元素的单位按原始反馈保留，不额外换算。所有状态命令支持 `--interval`，除底盘默认持续打印外，其余默认查询一次。

## 相机

相机使用独立 Bridge WebSocket（默认 `10.192.1.4:443`），解析 BRDG v1 图像消息。OpenCV 负责解码和窗口显示，单路默认顶部相机，三路为左/右/顶部：

```bash
# 实机使用自签名证书时显式加 --insecure；只用于你信任的机器人网络
./build/highlevel/camera_stream --insecure
./build/highlevel/camera_stream_3 --fps 60 --insecure
./build/highlevel/camera_websocket_benchmark --duration 10 --insecure
```

默认验证 TLS 证书。吞吐测试无需桌面、不解码图像、不链接 OpenCV；它统计传输帧率/吞吐，不代表解码显示帧率。相机具体参数见对应 `--help`。每路接收使用独立连接，显示只取最新帧；关闭窗口、q、Esc、Ctrl+C 退出。

## 验证范围

`ctest` 默认跑两类不连接机器人的检查：离线协议/键盘状态测试，以及 21 个示例各自的 `--help`。额外的本机 WebSocket 模拟服务测试可执行：

```bash
./build/highlevel/tron2_highlevel_tests --network
```

该测试只监听 `127.0.0.1` 随机端口，验证请求匹配、失败响应、超时等。编译及模拟服务测试不能替代实机运动、固件兼容性和相机显示验证。
