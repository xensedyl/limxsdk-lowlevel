# TRON2 Python 底层接口示例

本目录演示如何使用 `limxsdk` Python wheel 初始化 TRON2、订阅底层状态以及发布原始关节命令。

底层接口直接面向电机状态和控制命令。尤其是 `publishRobotCmd.py`，会以 300 Hz 连续发送位置、增益和力矩字段；错误的目标、关节顺序或增益可能导致机器人快速运动或失稳。请在熟悉控制接口、确认急停有效并清空机器人运动范围后使用。

## 安装 SDK

在仓库根目录根据平台安装对应 wheel。

Linux x86_64：

```bash
python3 -m pip install ./python3/amd64/limxsdk-4.0.2-py3-none-any.whl
```

Linux aarch64：

```bash
python3 -m pip install ./python3/aarch64/limxsdk-4.0.2-py3-none-any.whl
```

验证安装：

```bash
python3 -c "import limxsdk; print(limxsdk.__file__)"
```

`limxsdk 4.0.2` 要求 `numpy>1.21.0,<1.26.4`。如果还要在同一环境运行高层相机示例，建议使用 `numpy==1.26.3`。

## IP 参数和运行方式

所有底层示例都接受一个可选的位置参数作为机器人 IP：

```bash
python3 python3/examples/lowlevel/getMotorNumber.py [robot_ip]
```

- 真机默认地址：`10.192.1.2`
- 本地仿真通常使用：`127.0.0.1`

例如：

```bash
python3 python3/examples/lowlevel/getMotorNumber.py 10.192.1.2
python3 python3/examples/lowlevel/subscribeRobotState.py 10.192.1.2
```

持续订阅和发布的示例使用 `Ctrl+C` 退出。

## 示例说明

| 文件 | 功能 | 是否发送控制命令 |
| --- | --- | --- |
| `getMotorNumber.py` | 初始化机器人并读取电机数量 | 否 |
| `subscribeRobotState.py` | 订阅关节位置 `q`、速度 `dq` 和力矩 `tau` | 否 |
| `subscribeImuData.py` | 订阅加速度、角速度和姿态四元数 | 否 |
| `subscribeDiagnosticValue.py` | 订阅诊断级别、代码和消息 | 否 |
| `subscribeSensorJoy.py` | 订阅遥控器轴和按键数据 | 否 |
| `publishRobotCmd.py` | 以 300 Hz 连续发布 16 电机原始命令 | **是，高风险** |

建议按照以下顺序验证：

```bash
# 1. 确认 SDK 能连接并识别电机数量
python3 python3/examples/lowlevel/getMotorNumber.py 10.192.1.2

# 2. 只读检查机器人状态
python3 python3/examples/lowlevel/subscribeRobotState.py 10.192.1.2

# 3. 按需检查 IMU、诊断信息和遥控器
python3 python3/examples/lowlevel/subscribeImuData.py 10.192.1.2
python3 python3/examples/lowlevel/subscribeDiagnosticValue.py 10.192.1.2
python3 python3/examples/lowlevel/subscribeSensorJoy.py 10.192.1.2
```

## 16 电机关节顺序

`RobotState.q/dq/tau` 和 `publishRobotCmd.py` 中的 16 维关节命令按以下顺序排列：

| 索引 | 关节名称 | 分组 |
| ---: | --- | --- |
| 0 | `abad_L_Joint` | 左臂 |
| 1 | `hip_L_Joint` | 左臂 |
| 2 | `yaw_L_Joint` | 左臂 |
| 3 | `knee_L_Joint` | 左臂 |
| 4 | `wrist_yaw_L_Joint` | 左臂 |
| 5 | `wrist_pitch_L_Joint` | 左臂 |
| 6 | `wrist_roll_L_Joint` | 左臂 |
| 7 | `abad_R_Joint` | 右臂 |
| 8 | `hip_R_Joint` | 右臂 |
| 9 | `yaw_R_Joint` | 右臂 |
| 10 | `knee_R_Joint` | 右臂 |
| 11 | `wrist_yaw_R_Joint` | 右臂 |
| 12 | `wrist_pitch_R_Joint` | 右臂 |
| 13 | `wrist_roll_R_Joint` | 右臂 |
| 14 | `head_pitch_Joint` | 头部 |
| 15 | `head_yaw_Joint` | 头部 |

关节位置和速度通常使用 rad 和 rad/s，力矩通常使用 N·m。请以当前机器人型号和 SDK 接口文档为准。

## 发布底层命令

运行 `publishRobotCmd.py` 之前，至少需要检查以下内容：

1. `getMotorNumber()` 返回 16；脚本检测到其他数量时会拒绝继续。
2. `TARGET_Q` 是当前机器人能够安全到达的 16 维关节位置，顺序与上表一致。
3. `Kp`、`Kd` 与机器人型号和负载匹配。
4. 机器人当前控制模式允许接收底层命令，急停和限位保护有效。
5. 开始发送时操作人员与设备均位于运动范围之外。

确认后运行：

```bash
python3 python3/examples/lowlevel/publishRobotCmd.py 10.192.1.2
```

该脚本会持续运行，不会自动停止。按 `Ctrl+C` 终止后，请继续确认机器人已经退出底层控制状态。

## 常见问题

### `ModuleNotFoundError: No module named 'limxsdk'`

确认安装 wheel 与运行示例时使用的是同一个 Python：

```bash
which python3
python3 -m pip show limxsdk
python3 -c "import limxsdk; print(limxsdk.__file__)"
```

### 初始化失败或收不到数据

- 确认机器人或仿真程序已经启动。
- 确认命令行传入的 IP 正确且网络可达。
- 检查防火墙、网卡和机器人控制服务状态。
- 先运行 `getMotorNumber.py`，将连接问题与订阅/发布逻辑分开排查。
