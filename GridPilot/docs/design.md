# GridPilot 智能巡逻小车控制中心 —— 设计文档

> 版本：v0.1（初稿）
> 日期：2026-11-23
> 状态：待评审（个人学习项目）

---

## 目录

1. [项目概述](#1-项目概述)
2. [需求规格](#2-需求规格)
3. [总体架构](#3-总体架构)
4. [通信协议设计](#4-通信协议设计)
5. [传输层设计](#5-传输层设计)
6. [模拟小车设计](#6-模拟小车设计)
7. [上位机模块详细设计](#7-上位机模块详细设计)
8. [关键类与接口汇总](#8-关键类与接口汇总)
9. [UI 布局设计](#9-ui-布局设计)
10. [文件与数据格式](#10-文件与数据格式)
11. [测试计划](#11-测试计划)
12. [工程目录结构](#12-工程目录结构)
13. [开发计划](#13-开发计划)
14. [风险与对策](#14-风险与对策)

---

## 1. 项目概述

### 1.1 项目简介

GridPilot 是一套"地面站 + 小车"系统的上位机部分，包含两个可执行程序：

- **GridPilot（地面站）**：Qt 桌面程序。提供网格地图编辑画布、小车实时位置显示、遥控操作、路径规划下发、遥测数据曲线与通信日志。
- **GridSim（模拟小车）**：Qt 控制台程序。模拟一台小车，响应地面站命令、维护位置运动模型、周期上报遥测数据。

两者通过 **TCP（网口/WiFi）或串口（USB-TTL）** 通信，报文格式为本文档第 4 章定义的自定义二进制协议。

### 1.2 项目目标

| # | 目标 |
|---|---|
| G1 | 完整走通"协议设计 → 编解码实现 → 双通道传输 → 图形界面"的全链路 |
| G2 | 地面站可编辑地图、遥控小车、下发路径、实时监控遥测 |
| G3 | 无硬件时，地面站 + 模拟小车两个程序对跑全功能可演示 |
| G4 | 换成真车（ESP32/Arduino）时，地面站代码零修改，只换传输参数 |
| G5 | 协议层有单元测试覆盖 |

### 1.3 非目标（本期不做）

- 视频图传
- 真实定位（GPS/UWB），位置由小车自报（模拟器中为运动模型推算）
- 多车管理（架构预留，本期只连一台）
- OTA 固件升级

---

## 2. 需求规格

### 2.1 功能需求

#### FR-1 连接管理

| 编号 | 需求 |
|---|---|
| FR-1.1 | 支持 TCP 连接（IP + 端口可配置） |
| FR-1.2 | 支持串口连接（端口名/波特率可配置，默认 115200） |
| FR-1.3 | 心跳保活：地面站 1s 发一次 HEARTBEAT，3 次无响应判掉线 |
| FR-1.4 | 断线自动重连（间隔 3s，可手动停止） |
| FR-1.5 | 状态栏显示连接状态指示灯与当前通道类型 |

#### FR-2 地图编辑

| 编号 | 需求 |
|---|---|
| FR-2.1 | 网格地图，尺寸可设（默认 20×15 格，格距 40px 逻辑尺寸） |
| FR-2.2 | 鼠标左键绘制障碍、右键擦除；支持拖动连续绘制 |
| FR-2.3 | 可摆放标记物（充电桩、目标点等图标） |
| FR-2.4 | 画布滚轮缩放（0.25×~4×）、按住中键/空格拖拽平移 |
| FR-2.5 | 地图保存/加载（JSON 格式，见 §10.1） |

#### FR-3 遥控

| 编号 | 需求 |
|---|---|
| FR-3.1 | 虚拟摇杆控制：输出左/右轮速度（-1000~1000） |
| FR-3.2 | 键盘 WASD/方向键等效遥控 |
| FR-3.3 | 遥控指令固定 20Hz 发送（MOVE_CMD），松开 200ms 后发零速停车指令 |
| FR-3.4 | 遥控指令不等待应答（fire-and-forget），不重传 |

#### FR-4 路径任务

| 编号 | 需求 |
|---|---|
| FR-4.1 | 地图模式下点击依次添加路径点（格子坐标） |
| FR-4.2 | 画布实时预览路径线（相邻点直线/折线） |
| FR-4.3 | 一键下发：路径点序列打包 GOTO_PATH 发给小车，收到 ACK 后标记"已下发" |
| FR-4.4 | 支持取消当前任务（发 GOTO_PATH 空列表） |

#### FR-5 遥测监控

| 编号 | 需求 |
|---|---|
| FR-5.1 | 小车 100ms 周期上报 TELEMETRY（电压/速度/位置/航向） |
| FR-5.2 | 电压、速度实时曲线（滚动窗口 60s） |
| FR-5.3 | 地图上小车图标平滑移动，尾迹线保留最近 500 点 |
| FR-5.4 | 电压低于阈值（3.3V）时界面告警（图标变红 + 状态栏提示） |

#### FR-6 日志

| 编号 | 需求 |
|---|---|
| FR-6.1 | 原始帧日志：时间戳、方向（TX/RX）、十六进制字节、解析后的命令名 |
| FR-6.2 | 日志分级：帧日志 / 事件日志（连接、错误）/ 调试日志 |
| FR-6.3 | 日志可导出为文本文件 |

#### FR-7 参数设置

| 编号 | 需求 |
|---|---|
| FR-7.1 | SET_PARAM 可设置：最大速度、低电压阈值（载荷=参数ID+值） |
| FR-7.2 | 读参数：READ_PARAM，应答带参数 ID 与当前值 |

### 2.2 非功能需求

| 编号 | 需求 |
|---|---|
| NFR-1 | 协议库（libs/protocol、libs/transport）不依赖 Qt Widgets，纯 QtCore，可独立单元测试 |
| NFR-2 | 遥控链路端到端延迟 < 100ms（本机回环） |
| NFR-3 | 遥测曲线 60s × 10Hz 数据量下界面不卡顿 |
| NFR-4 | 字节序统一大端（网络序），帧 CRC16/Modbus 校验 |
| NFR-5 | 所有超时、重连间隔等常量集中定义，不出现在业务代码里 |

---

## 3. 总体架构

### 3.1 分层架构

```
┌───────────────────────────────────────────────────────┐
│ GridPilot 地面站（app/）                               │
│                                                       │
│  UI 层   MainWindow | MapCanvas | TelemetryPanel      │
│          JoystickWidget | LogPanel | ConnectDialog    │
│  ───────────────────────────────────────────────────  │
│  业务层  DeviceManager（会话/心跳/重连）                │
│          MissionController（路径任务）                 │
│          TelelemetryStore（数据缓存，供曲线/地图消费） │
│  ───────────────────────────────────────────────────  │
│  协议层（libs/protocol，与 UI 无关）                    │
│          FrameCodec | Crc16 | CommandId | Payload 编解码│
│  ──────────────────────────────────────────────────── │
│  传输层（libs/transport，与协议、UI 无关）              │
│          ITransport ◄── TcpTransport                  │
│                     └── SerialTransport               │
└───────────────────────────────────────────────────────┘
            ↑ 同一套协议与传输库 ↑
┌───────────────────────────────────────────────────────┐
│ GridSim 模拟小车（simulator/，独立可执行）              │
│  SimVehicle（运动模型）| SimServer（会话）| 参数表      │
└───────────────────────────────────────────────────────┘
```

### 3.2 依赖方向（强制约束）

```
app ──► libs/protocol ──► QtCore
      libs/transport ──► QtCore (+ QtNetwork / QtSerialPort)
app ──► libs/transport
simulator ──► libs/protocol, libs/transport
```

- 协议层不知道传输层存在（只吃字节数组、吐 Frame）
- 传输层不知道协议层存在（只收发 QByteArray）
- 只有 app / simulator / tests 知道全部

### 3.3 数据流（以"遥控"与"遥测"为例）

```
遥控上行：
JoystickWidget --(speedL,speedR, 20Hz定时)--> MissionController
  --> FrameCodec.encode(MOVE_CMD) --> ITransport.send() --> 小车

遥测下行：
小车 100ms TELEMETRY --> ITransport 收到字节 --> FrameCodec.feed()（逐字节状态机）
  --> Frame 就绪信号 --> DeviceManager 分发 --> TelemetryStore 更新
  --> 信号通知 MapCanvas（移动图标）与 TelemetryPanel（追加曲线点）
```

---

## 4. 通信协议设计

### 4.1 帧格式

所有多字节字段**大端序**（网络字节序）。

```
偏移  长度  字段        说明
0     2    SYNC        帧头，固定 0xAA 0x55
2     1    LEN         载荷长度（0~255，不含帧头/CRC）
3     1    CMD         命令字（见 §4.2）
4     2    SEQ         序号，发送方递增（应答帧回填请求帧的 SEQ）
6     N    PAYLOAD     载荷，N = LEN
6+N   2    CRC16       CRC-16/Modbus，计算范围：LEN 起至载荷末尾
                        （不含 SYNC 与 CRC 本身）
```

帧总长 = 8 + LEN 字节。

### 4.2 命令字定义

| 值 | 名称 | 方向 | 载荷 | 说明 |
|---|---|---|---|---|
| 0x01 | HEARTBEAT | 双向 | 无 | 心跳；应答回同 SEQ |
| 0x02 | MOVE_CMD | 站→车 | 4B | 左轮速度 int16 + 右轮速度 int16 |
| 0x03 | GOTO_PATH | 站→车 | 1+3N B | 点数 N(uint8)，每点 3B：col(uint8)+row(uint8)+保留(uint8) |
| 0x04 | SET_PARAM | 站→车 | 6B | 参数ID(uint16) + 值(int32) |
| 0x05 | READ_PARAM | 站→车 | 2B | 参数ID(uint16) |
| 0x06 | TELEMETRY | 车→站 | 14B | 见 §4.3 |
| 0x07 | PARAM_VALUE | 车→站 | 6B | 参数ID(uint16) + 值(int32)，READ_PARAM 应答 |
| 0x08 | ACK | 双向 | 3B | 被应答的CMD(1B) + 状态(1B) + 保留(1B)；状态 0x00=OK，0x01=拒绝 |
| 0x09 | ERROR | 双向 | 1B | 错误码（见 §4.5） |

### 4.3 TELEMETRY 载荷布局（14B）

```
偏移  类型    字段
0     uint16  battery_mV     电池电压，单位 mV
2     int16   speedL         左轮实际速度（-1000~1000）
4     int16   speedR         右轮实际速度
6     uint16  odometry_cm    累计里程，单位 cm（回绕）
8     uint8   col            当前所在格：列
9     uint8   row            当前所在格：行
10    int16   heading        航向角，0.1° 为单位（0~3599）
12    uint8   state          状态：0x00 空闲 / 0x01 遥控 / 0x02 巡航 / 0x03 充电
13    uint8   flags          位标志：bit0 低电压告警
```

### 4.4 收发规则

1. **请求-应答类**（HEARTBEAT / GOTO_PATH / SET_PARAM / READ_PARAM）：发送后启动 500ms 超时，未收到 ACK/对应应答则重发，最多重发 2 次，仍失败报通信错误。
2. **流类**（MOVE_CMD / TELEMETRY）：不重传。丢帧由下一个周期的新数据自然覆盖。
3. **SEQ 去重**：接收方对请求类命令检查 SEQ，重复 SEQ 直接丢弃不重复执行。
4. **帧同步恢复**：解析状态机遇到非法帧（LEN 超限、CRC 错）时丢弃该帧并回到 hunting 状态，在后续字节流中重新搜索 0xAA 0x55。

### 4.5 错误码（ERROR 载荷）

| 值 | 含义 |
|---|---|
| 0x01 | CRC 校验失败 |
| 0x02 | 不支持的命令字 |
| 0x03 | 载荷长度非法 |
| 0x04 | 参数 ID 不存在 |
| 0x05 | 值超范围 |
| 0x06 | 忙（任务执行中拒绝新命令） |

---

## 5. 传输层设计

### 5.1 ITransport 抽象接口

```cpp
class ITransport : public QObject {
    Q_OBJECT
public:
    enum class State { Disconnected, Connecting, Connected };
    virtual ~ITransport() = default;
    virtual void open(const QVariantMap& params) = 0; // TCP: host/port 串口: port/baud
    virtual void close() = 0;
    virtual void send(const QByteArray& data) = 0;
    virtual State state() const = 0;
signals:
    void dataReceived(const QByteArray& data);   // 收到原始字节（可能任意长度分片）
    void stateChanged(ITransport::State s);
    void errorOccurred(const QString& what);
};
```

**约定**：`dataReceived` 的分片粒度不做任何保证（可能半帧、多帧、跨帧混合），帧重组是协议层的责任。

### 5.2 TcpTransport

- `QTcpSocket`；`open()` 发起连接，`connectTimeout` 3s
- 收：`readyRead` → `socket->readAll()` → 发 `dataReceived`
- 发：直接 `socket->write`
- 断线：`errorOccurred`/`disconnected` → `stateChanged(Disconnected)`，由上层 DeviceManager 决定重连

### 5.3 SerialTransport

- `QSerialPort`；参数：端口名、波特率（默认 115200）、8N1
- 收发逻辑同上；串口被占用时 `errorOccurred("port busy")`

### 5.4 通道选择

UI 层"连接设置"对话框产出 `QVariantMap` 参数，`TransportFactory::create(type, params)` 返回对应实例。业务代码只面对 `ITransport`。

---

## 6. 模拟小车设计（GridSim）

### 6.1 职责

扮演一台真实小车：TCP 监听（默认 127.0.0.1:9000）+ 串口透传双通道，内部状态机与运动模型。

### 6.2 内部组成

| 组件 | 职责 |
|---|---|
| SimServer | 挂 ITransport，跑 FrameCodec，命令分发（与地面站 DeviceManager 镜像） |
| SimVehicle | 运动模型：双轮差速模型，tick 50ms 更新位置/航向；撞障碍则停并上报 ERROR |
| ParamTable | 参数表（最大速度、低电压阈值），响应 SET/READ |
| TelemetryTimer | 100ms 定时组 TELEMETRY 帧发送 |
| 电池模型 | 电压从 4.2V 随时间缓慢下降（可配置快慢），模拟续航 |

### 6.3 命令行参数

```
GridSim [--tcp port] [--serial COM7] [--battery-fast] [--map map.json]
```

---

## 7. 上位机模块详细设计

### 7.1 协议层（libs/protocol）

#### FrameCodec（无状态编解码 + 有状态流解析）

```cpp
class FrameCodec : public QObject {
    Q_OBJECT
public:
    // 编码：结构 → 字节帧（含 CRC）
    static QByteArray encode(quint8 cmd, quint16 seq, const QByteArray& payload);
    // 流式喂字节：内部状态机，凑齐完整帧发信号；坏帧发 badFrame 信号后自动重同步
    void feed(const QByteArray& data);
signals:
    void frameReady(quint8 cmd, quint16 seq, const QByteArray& payload);
    void badFrame(const QString& reason);   // CRC错/长度非法，用于日志与统计
};
```

**解析状态机**：

```
HUNT_SYNC → (0xAA) → (0x55) → READ_LEN → READ_HDR(cmd+seq)
          → READ_PAYLOAD(LEN 字节，收满) → CHECK_CRC ──通过──► frameReady，回 HUNT_SYNC
                                                  └─失败──► badFrame，回 HUNT_SYNC
```

实现要点：内部用固定缓冲 + 写入偏移；LEN 合法区间 [0, 255]；CRC 查表法（256 项表，编译期生成或初始化一次）。

#### 各载荷的编解码

每个命令字提供一对纯函数（放在 `payloads.h`）：

```cpp
struct MoveCmdPayload { qint16 speedL; qint16 speedR;
    QByteArray encode() const;
    static std::optional<MoveCmdPayload> decode(const QByteArray&); };
struct TelemetryPayload { quint16 batteryMv; qint16 speedL, speedR;
    quint16 odometryCm; quint8 col, row; qint16 heading; quint8 state, flags;
    /* encode/decode 同上 */ };
// ... 其余命令同理
```

decode 返回 `std::optional`，载荷非法时上层回 ERROR(0x03)。

#### RequestManager（请求-应答匹配）

```cpp
class RequestManager : public QObject {
public:
    void request(quint8 cmd, const QByteArray& payload,
                 std::function<void(Reply)> callback, int timeoutMs = 500);
    // Reply { bool ok; quint8 ackCmd; QByteArray payload; QString error; }
    // 内部：挂起表<SEQ, 上下文>，QTimer 超时重发，最多 2 次
public slots:
    void onFrame(...);   // 与 FrameCodec::frameReady 连接，负责匹配挂起请求
};
```

### 7.2 业务层

#### DeviceManager

- 持有 ITransport + FrameCodec + RequestManager
- 心跳 QTimer（1s），连续 3 次超时 → 掉线处理
- 掉线自动重连 QTimer（3s），重连时重建传输对象
- 把 frameReady 翻译成业务信号：`telemetryUpdated(TelemetryPayload)`、`paramValue(id, value)`、`errorRaised(code)`

#### MissionController

- 路径点列表（QVector<QPoint>），管理"编辑中/已下发/执行中"状态
- 下发：GOTO_PATH（点数>255 截断并提示），走 RequestManager
- 取消：下发空路径

#### TelemetryStore

- 环形缓冲：每通道（电压/速度）最近 600 点（60s × 10Hz）
- 尾迹坐标环形缓冲 500 点
- 发 `seriesUpdated` 供曲线面板取数（UI 定时 100ms 拉取，避免每点一信号刷爆）

### 7.3 UI 层

#### MapCanvas（核心，QGraphicsView 体系）

| 元素 | 实现 |
|---|---|
| 网格 | 自定义 `GridBackgroundItem`（最底层），paint() 中按缩放级别画线，格子越界不画 |
| 障碍格 | `ObstacleCellItem`，一个格子一个实例；paint 画填充+描边；右键擦除即 remove |
| 标记物 | `MarkerItem`（QGraphicsPixmapItem 或自绘），可拖动 |
| 小车 | `VehicleItem`，自绘三角形车头 + 轮廓；位置变化用 QPropertyAnimation 插值（100ms 步进对齐遥测周期） |
| 尾迹 | `TrailPathItem`，持有 QPolygonF，追加点 setPath 重画 |
| 路径预览 | `RoutePathItem`，折线；路径点显示为可拖拽圆点 `WaypointItem` |

**交互模式**（枚举，工具栏切换）：`Edit（编辑障碍/标记）→ Route（点选路径点）→ Monitor（只读监控）`。

**坐标换算**：`mapToScene` 后除以格距得格子坐标（col = x/CELL, row = y/CELL），注意缩放因子已被 view 的 transform 处理。

**缩放**：滚轮 `wheelEvent` 中 `view->scale(factor)`，以光标为锚点（`AnchorUnderMouse`），限制 0.25~4 倍。

#### JoystickWidget（自绘控件）

- `paintEvent` 画底盘圆 + 手柄圆；`mousePress/Move/Release` 限幅拖动
- 归一化输出 (dx, dy) ∈ [-1,1]²，换算左右轮速：`L = (dy + dx) * MAX, R = (dy - dx) * MAX`（差速）
- 20Hz 对外发 `cmdChanged(l, r)`（QTimer 节流），手柄回中 200ms 后发一次零速

#### TelemetryPanel

- 曲线：QtCharts（QLineSeries，定时 100ms replace 全量最近 600 点，关掉动画/抗锯齿开销可控）
- 电压数字 + 低电压红色告警条

#### LogPanel

- QPlainTextEdit（`MaximumBlockCount` 限 5000 行防爆内存）
- 三档过滤器（复选框）：帧 / 事件 / 调试
- 帧日志格式：`14:32:01.123 TX 10B AA 55 04 00 01 00 03 ... CRC=8A21  [GOTO_PATH]`

#### ConnectDialog

- 通道类型单选 TCP/串口 → 动态出参数控件 → 产出 QVariantMap 交给 TransportFactory

---

## 8. 关键类与接口汇总

| 层 | 类 | 一句话职责 |
|---|---|---|
| 协议 | `Crc16` | CRC-16/Modbus 查表计算 |
| 协议 | `FrameCodec` | 字节流 ⇄ 帧（编码静态函数 + 流式解码状态机） |
| 协议 | `payloads.h` | 各命令字载荷结构体与编解码 |
| 协议 | `RequestManager` | 请求-应答匹配、超时重传 |
| 传输 | `ITransport` | 传输抽象接口 |
| 传输 | `TcpTransport` / `SerialTransport` | 两种实现 |
| 传输 | `TransportFactory` | 按参数创建实例 |
| 业务 | `DeviceManager` | 会话生命周期：连接、心跳、重连、分发 |
| 业务 | `MissionController` | 路径任务状态与下发 |
| 业务 | `TelemetryStore` | 遥测/尾迹环形缓冲 |
| UI | `MainWindow` | 布局、菜单、工具栏、装配各组件 |
| UI | `MapCanvas` | 地图画布与全部图形 item |
| UI | `JoystickWidget` | 虚拟摇杆自绘控件 |
| UI | `TelemetryPanel` / `LogPanel` / `ConnectDialog` | 曲线、日志、连接设置 |

**信号槽关键连接**（MainWindow::connectAll 中集中装配）：

```
ITransport::dataReceived ──────► FrameCodec::feed
FrameCodec::frameReady ────────► DeviceManager::onFrame（→ 各业务信号）
FrameCodec::badFrame ──────────► LogPanel（统计坏帧）
JoystickWidget::cmdChanged ────► MissionController::sendMove
DeviceManager::telemetryUpdated ► TelemetryStore::append（→ 定时刷新曲线/地图）
DeviceManager::stateChanged ───► 状态栏指示灯 / 重连提示
```

---

## 9. UI 布局设计

```
┌────────────────────────────────────────────────────────────┐
│ 菜单栏  文件(地图打开/保存/退出)  连接(TCP/串口/断开)  帮助   │
├────────────────────────────────────────────────────────────┤
│ 工具栏  [连接▾] [断开] | [编辑模式] [路径模式] [监控模式] |   │
│         [清空障碍] [下发路径] [取消任务]                     │
├───────────────────────────────────────┬────────────────────┤
│                                       │  左：地图画布        │
│                                       │  右（QTabWidget）：  │
│                                       │  ┌───────────────┐ │
│                                       │  │ Tab1 遥控      │ │
│                                       │  │   摇杆 + 键说明 │ │
│                                       │  ├───────────────┤ │
│                                       │  │ Tab2 遥测      │ │
│                                       │  │   电压/速度曲线 │ │
│                                       │  ├───────────────┤ │
│                                       │  │ Tab3 日志      │ │
│                                       │  │   帧日志+过滤   │ │
│                                       │  └───────────────┘ │
├───────────────────────────────────────┴────────────────────┤
│ 状态栏  ●已连接(TCP 127.0.0.1:9000)  电压4.05V  里程12.3m   │
│         任务:巡航中(3/5点)              坏帧:0              │
└────────────────────────────────────────────────────────────┘
```

默认窗口 1280×800，最小 1024×640；左右分栏 QSplitter 可拖。

---

## 10. 文件与数据格式

### 10.1 地图文件（JSON，UTF-8）

```json
{
  "version": 1,
  "cols": 20,
  "rows": 15,
  "obstacles": [[3,2],[3,3],[10,7]],
  "markers": [ {"type": "charger", "col": 18, "row": 13} ]
}
```

### 10.2 连接配置（QSettings 持久化）

```
connection/type=tcp|serial
connection/host=127.0.0.1
connection/port=9000
connection/serialName=COM7
connection/baud=115200
ui/lastMapPath=...
```

---

## 11. 测试计划

| 层 | 内容 | 工具 |
|---|---|---|
| 协议单测 | CRC 已知向量（如 `01 04 00 01` → 已知 CRC 值）；encode→feed 回环解出原帧；粘包（两帧连发一次 feed）、半包（逐字节喂）、坏 CRC、LEN 超限 | QtTest，`tests/tst_protocol` |
| 传输 | 本机回环：两个 TcpTransport 对发；串口用一对虚拟串口（com0com）或两台真机 | `tests/tst_transport` |
| 集成 | 地面站 ↔ GridSim 全场景脚本：连接→心跳→遥控→读改参数→下发路径→断电（关模拟器）→重连 | 手动用例清单 + 自动化 smoke |
| 性能 | 10Hz 遥测持续 30min：内存无增长、曲线不卡 | 任务管理器 + 操作观察 |

**坏帧用例必须有**：随机翻转载荷一个字节 → 应收 ERROR(0x01) 或 badFrame，程序不崩溃。

---

## 12. 工程目录结构

```
GridPilot/
├── CMakeLists.txt              # 顶层（add_subdirectory 各模块）
├── docs/
│   ├── 设计文档.md             # 本文档
│   └── protocol.md             # 协议速查卡（帧格式+命令字，给写固件时对照）
├── libs/
│   ├── protocol/
│   │   ├── CMakeLists.txt
│   │   ├── include/protocol/   # frame_codec.h, crc16.h, payloads.h, command_id.h
│   │   └── src/
│   └── transport/
│       ├── CMakeLists.txt
│       ├── include/transport/  # i_transport.h, tcp_transport.h, serial_transport.h
│       └── src/
├── app/                        # GridPilot 地面站
│   ├── CMakeLists.txt
│   ├── main.cpp
│   ├── business/               # device_manager, mission_controller, telemetry_store
│   └── ui/                     # main_window, map_canvas(+items), joystick_widget,
│                               # telemetry_panel, log_panel, connect_dialog
├── simulator/                  # GridSim 模拟小车
│   ├── CMakeLists.txt
│   └── src/                    # main, sim_server, sim_vehicle, param_table
└── tests/
    ├── tst_protocol/
    └── tst_transport/
```

---

## 13. 开发计划

> 节奏按每周 5~8 小时业余时间估算；每阶段结束 git tag + 简短小结。

| 阶段 | 周 | 内容 | 退出标准（能演示什么） |
|---|---|---|---|
| 0 底座 | 1 | CMake 骨架、git、协议文档定稿、QtSerialPort 环境就绪 | 空窗口编译运行，commit 流程跑通 |
| 1 模拟车 | 2~3 | FrameCodec encode + Crc16；SimServer TCP 收发；PING/ACK；参数表 | 测试工具（或单测）ping 通模拟车并读写参数成功 |
| 2 协议库 | 4~5 | 流式解码状态机、粘包/坏帧用例；SerialTransport；RequestManager 超时重传 | tst_protocol 全绿；TCP/串口即插即用 |
| 3 画布 | 6~8 | MapCanvas 全套：网格、障碍编辑、缩放平移、VehicleItem 动画、尾迹、模式切换 | 遥控模拟车，地图上图标实时移动、画尾迹 |
| 4 监控 | 9 | 摇杆控件、遥测曲线、心跳重连、日志面板 | 拔线重连 + 曲线滚动 + 帧日志完整 |
| 5 任务 | 10 | 路径点编辑与下发、巡航状态显示、收尾打磨（图标、配置持久化） | 画路径→下发→模拟车自动巡航，全程演示 |
| 6 真车(可选) | 11+ | ESP32/Arduino 固件按 protocol.md 实现，替换模拟车 | 地面站不改代码控制真车 |

---

## 14. 风险与对策

| # | 风险 | 对策 |
|---|---|---|
| R1 | 粘包/半包解析写错，偶发丢帧难复现 | 阶段 2 就写逐字节喂入的单测；坏帧计数进日志 |
| R2 | QtCharts 大数据量卡顿 | 定时全量替换而非逐点 append；必要时换 QCustomPlot |
| R3 | 串口环境依赖硬件 | 开发全程用 TCP + 模拟车，串口放阶段 2 末与真机阶段验证 |
| R4 | 画布交互（缩放锚点、坐标换算）细节多 | 每个交互单独小步提交；参照 Qt 官方 Elastic Nodes / Chip 例程 |
| R5 | 范围蔓延（功能越加越多） | 以 §13 退出标准为准，新想法记进 docs/ideas.md 不进本期 |
| R6 | 真车阶段固件侧调试难 | 模拟车就是协议基准：真车行为对不齐时，用地面站帧日志对比模拟车日志定位 |

---

## 附录 A：待定项（TBD）

- [ ] 串口防粘包：无帧头保护时（线路噪声）是否加"连续 SYNC 双确认"？——阶段 2 实测定
- [ ] 路径点是否需要带速度（每点 4B 版载荷）？——阶段 5 视巡航体验定
- [ ] 真车选型：ESP32（自带 WiFi，推荐）或 Arduino+NRF24/蓝牙

---

*本文档随开发推进滚动更新；每次协议变更必须先改 §4 并打版本号（协议版本记录在 CHANGELOG）。*
