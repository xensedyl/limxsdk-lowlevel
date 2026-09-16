# TRON2 Python 上层接口示例

本目录提供基于 WebSocket 的 TRON2 上层控制、状态读取和相机视频流示例。上层控制代码封装在 `robot_utils.py` 中，不依赖 `limxsdk` wheel。

## 使用前准备

- 确保开发机能够访问机器人控制服务 `10.192.1.2:5000`。
- 相机示例还需要能够访问 Bridge 服务 `10.192.1.4:443`。
- 控制类示例会让真实机器人或夹爪运动。运行前请清空运动范围、准备急停，并先使用只读示例确认连接和状态正常。
- 示例中的机器人 IP、目标位置和运行时间均为示例值，请在运行前检查脚本顶部的配置常量。

建议在虚拟环境中安装本目录全部示例所需的依赖：

```bash
python3 -m pip install \
    "numpy==1.26.3" \
    "opencv-python==4.11.0.86" \
    websocket-client \
    websockets
```

`limxsdk 4.0.2` 要求 `numpy>1.21.0,<1.26.4`。如果上层和底层示例使用同一个 Python 环境，请保留上述 NumPy 版本约束。

## 运行方式

以下命令均在仓库根目录执行：

```bash
cd /path/to/limxsdk-lowlevel
python3 python3/examples/highlevel/get_joint_state.py
```

上层运动和状态脚本默认连接 `10.192.1.2`。移动平台脚本支持 `--host`、`--port`、`--accid` 和 `--timeout`。其他脚本如需修改地址，请编辑脚本顶部的 `ROBOT_IP`。相机脚本可通过 `--host` 指定 Bridge 地址。

## 示例说明

### 状态读取

| 文件 | 功能 | 是否产生运动 |
| --- | --- | --- |
| `get_joint_state.py` | 读取双臂、夹爪和头部的 18 维状态 | 否 |
| `get_ee_pose.py` | 读取左右臂末端位置和四元数，位姿格式为 `xyz+wxyz` | 否 |
| `get_gripper_state.py` | 读取左右二指夹爪的开口度、速度和力状态 | 否 |

建议首次连接机器人时先运行：

```bash
python3 python3/examples/highlevel/get_joint_state.py
python3 python3/examples/highlevel/get_ee_pose.py
python3 python3/examples/highlevel/get_gripper_state.py
```

### 升降台和底盘（仅适用于移动版双臂）

以下脚本实现[官方 SDK 文档第 3.6.11～3.6.17 节](https://www.limxdynamics.com/zh/documents/847884267345285120#3.6.11-获取升降台状态（仅适用于移动版双臂）)的接口。它们使用 `websocket-client`，不需要 NumPy、OpenCV 或 `limxsdk`：

- `get_lifter_state.py`：获取升降台 `q` 和 `v` 状态。
- `set_lifter_position.py`：设置升降台绝对位置，单位 mm。
- `set_lifter_velocity.py`：设置升降台速度，单位 mm/s。
- `get_lifter_position.py`：获取升降台位置、`q`、`q_per_mm` 和数据时间戳。
- `get_chassis_state.py`：获取底盘线速度、角速度和转向角。
- `set_chassis_mode.py`：设置 `ackerman`、`parallel`、`park`、`spinning` 或 `emergency_stop` 模式。
- `chassis_move.py`：发送 `x`、`y`、`yaw` 底盘运动值，范围均为 `[-1, 1]`。
- `mobile_platform_common.py`：上述脚本共用的连接和协议模块，不需要直接运行。

先安装依赖：

```bash
python3 -m pip install websocket-client
```

读取升降台状态或当前位置：

```bash
# 查询一次
python3 python3/examples/highlevel/get_lifter_state.py
python3 python3/examples/highlevel/get_lifter_position.py

# 每 0.5 秒查询一次，按 Ctrl+C 退出
python3 python3/examples/highlevel/get_lifter_state.py --interval 0.5
python3 python3/examples/highlevel/get_lifter_position.py --interval 0.5
```

控制升降台：

```bash
# 2000 ms 内移动到绝对位置 200.5 mm；duration 可以为 0
python3 python3/examples/highlevel/set_lifter_position.py 200.5 2000

# 以 50 mm/s 运动 2000 ms；速度可正可负，duration 必须大于 0
python3 python3/examples/highlevel/set_lifter_velocity.py 50 2000
```

读取底盘状态：

```bash
# 查询一次
python3 python3/examples/highlevel/get_chassis_state.py

# 每次收到响应后等待 0.5 秒，按 Ctrl+C 退出
python3 python3/examples/highlevel/get_chassis_state.py --interval 0.5
```

设置底盘模式并发送运动命令：

```bash
# 设置阿克曼模式
python3 python3/examples/highlevel/set_chassis_mode.py ackerman

# 发送 x=0.3、y=0、yaw=0 的运动命令
python3 python3/examples/highlevel/chassis_move.py 0.3 0 0

# 发送全零运动命令
python3 python3/examples/highlevel/chassis_move.py 0 0 0

# 设置急停模式；--yes 用于明确跳过交互确认
python3 python3/examples/highlevel/set_chassis_mode.py emergency_stop --yes
```

控制脚本会先显示参数，只有输入 `yes` 后才连接并发送；明确用于自动化时可增加 `--yes`。`chassis_move.py` 每次只发送一条请求，机器人收到命令后的保持行为取决于控制器版本；停止运动时发送 `0 0 0`，紧急情况使用机器人急停装置或 `emergency_stop` 模式。底盘运动模式与 `x/y/yaw` 的组合含义也取决于控制器，请先设置所需模式，再发送运动命令。

所有移动平台脚本默认从 `notify_robot_info` 自动获取 ACCID。通过 `--host 10.192.1.2 --port 5000` 指定服务地址，`--timeout 5` 设置各阶段超时；如果无法自动获取 ACCID，可传入 `--accid <机器人实际序列号>`。

`get_chassis_state.py` 输出响应时间戳，以及 `linear_velocity`（线速度）、`angular_velocity`（角速度）、`steering_angle`（转向角）。这三个字段对应响应内 `data.data` 的三个元素；文档该节未标注单位，脚本保留原始数值。`get_lifter_state.py` 中 `q` 和 `v` 的单位在文档该节也未标注，脚本同样保留原始数值。

### 运动与夹爪控制

| 文件 | 功能 | 主要配置 |
| --- | --- | --- |
| `moveh.py` | 头部 pitch、yaw 插值运动 | `TARGET_HEAD`、`MOVE_TIME` |
| `movej.py` | 双臂 14 维关节空间插值运动 | `TARGET_JOINTS`、`MOVE_TIME` |
| `movep.py` | 双臂末端笛卡尔空间插值运动 | `LEFT_Z_OFFSET`、`MOVE_TIME` |
| `servoj.py` | 双臂和头部 16 维高频关节伺服 | `SERVO_RATE`、`RUN_TIME`、`AMPLITUDE` |
| `servop.py` | 双臂末端连续位姿伺服 | `SERVO_RATE`、`RUN_TIME`、`AMPLITUDE` |
| `gripper_control.py` | 设置左右夹爪开口度、速度和力 | `LEFT_*`、`RIGHT_*` |
| `moveHeadWebSocket.py` | 直接演示 MoveH WebSocket 请求 | `ROBOT_IP`、`ROBOT_PORT` |
| `websocket_client.py` | 与 `moveHeadWebSocket.py` 相同的交互式 MoveH 示例 | `ROBOT_IP`、`ROBOT_PORT` |

运动示例在发送命令前会等待用户按 Enter 确认。例如：

```bash
python3 python3/examples/highlevel/movej.py
```

注意事项：

- `movej.py` 的目标顺序是左臂 7 个关节，然后是右臂 7 个关节，单位为 rad。
- `moveh.py` 的头部顺序是 `[pitch, yaw]`，单位为 rad。
- `movep.py` 和 `servop.py` 使用 `xyz+wxyz` 位姿格式，位置单位为 m。
- ServoJ 建议在实时系统中以不低于 500 Hz 的频率发送。普通 Python/Linux 环境不能保证硬实时，本示例主要用于接口演示。
- 按 `Ctrl+C` 可中止示例；中止后仍应确认机器人已经退出运动或伺服状态。

### 相机视频流

| 文件 | 功能 | 图形界面要求 |
| --- | --- | --- |
| `camera_stream.py` | 接收并显示单路相机视频 | 需要 OpenCV GUI |
| `camera_stream_3.py` | 使用三个线程接收并显示左、右、顶部相机 | 需要 OpenCV GUI |
| `camera_websocket_benchmark.py` | 统计三路压缩图像的接收帧率和带宽，不解码、不显示 | 不需要 OpenCV GUI |

单路顶部相机：

```bash
python3 python3/examples/highlevel/camera_stream.py \
    --host 10.192.1.4 \
    --topic /camera/top/color/image_raw/compressed \
    --fps 30
```

同时显示三路相机：

```bash
python3 python3/examples/highlevel/camera_stream_3.py \
    --host 10.192.1.4 \
    --fps 31
```

在任意视频窗口中按 `q` 或 `Esc` 退出。

测试三路 WebSocket 纯接收性能：

```bash
python3 python3/examples/highlevel/camera_websocket_benchmark.py \
    --host 10.192.1.4 \
    --duration 20 \
    --warmup 3
```

如需在测试结束后保存压缩帧，可增加 `--save-dir <目录>`。保存模式会先把帧保存在内存中，长时间测试可能消耗大量内存。

## 辅助模块

- `robot_utils.py`：WebSocket 连接、状态请求和 MoveJ/ServoJ/MoveP/ServoP 等接口封装。
- `example_common.py`：示例共用的连接等待、状态切片和末端位姿转换函数。
- `mobile_platform_common.py`：移动版双臂的同步 WebSocket 请求、响应匹配和参数辅助函数。

这些文件由其他示例导入，通常不需要直接运行。

## 常见问题

### OpenCV 没有 `waitKey` 或 `destroyAllWindows`

这通常表示 `opencv-python` 与 `opencv-python-headless` 冲突，或者卸载其中一个包后留下了不完整的 `cv2` 目录。不要在同一环境中同时安装两个版本，可重新安装 GUI 版本：

```bash
python3 -m pip uninstall -y opencv-python opencv-python-headless
python3 -m pip install --no-cache-dir "opencv-python==4.11.0.86"
```

验证安装：

```bash
python3 -c "import cv2; print(cv2.__version__); print(hasattr(cv2, 'waitKey'))"
```

最后一项应为 `True`。如果在无桌面的服务器、容器或 SSH 会话中运行，请使用 `camera_websocket_benchmark.py`，或自行改为保存/转发图像；此时无法使用 OpenCV 窗口。

### 连接超时或拒绝连接

检查本机到机器人和 Bridge 的网络连通性：

```bash
ping 10.192.1.2
ping 10.192.1.4
```

同时确认机器人控制服务和相机 Bridge 已启动，且 IP 与脚本配置一致。

### 升降台接口返回 `fail_no_data`

如果 `get_lifter_state.py` 返回 `fail_no_data`，表示机器人已收到请求，但当前没有可提供的升降台数据。底盘状态能够正常返回只能证明底盘 WebSocket 服务正常，不能证明升降台模块存在或已经初始化。请依次确认：

- 当前机器人确实是带升降台的移动版双臂型号；普通双臂或没有升降台的移动版不支持这些数据。
- 升降台控制器、电源和通信线已连接，机器人启动后已完成初始化。
- 机器人主控软件版本包含文档 3.6.11～3.6.14 接口。

也可以再查询一次当前位置：

```bash
python3 python3/examples/highlevel/get_lifter_position.py
```

如果两个查询都返回 `fail_no_data`，问题在机器人型号、硬件或主控软件状态，重新运行 Python 脚本不会生成升降台数据。
