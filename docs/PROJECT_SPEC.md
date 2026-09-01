# 项目规范

## 项目目标与事实标准

本项目是一个可重复验证的实验型 CAN--MQTT 网关：STM32F103C8T6 产生确定性的
模拟车辆数据，经 500 kbit/s 物理 CAN 总线进入 i.MX6ULL，再由 Linux 网关上传到
PC 端 Mosquitto Broker。项目价值以可复现测试和保留证据为准，不以源码目录看起来
是否“完整”为准。

任何编译、烧录、上板、网络、可靠性、时延、CPU、内存或运行时长结论，原则上都必须有
对应命令和 `artifacts/<run_id>/` 证据。若项目所有者明确选择简化验收，文档必须同时
标明“项目所有者确认、原始日志未归档”，且不得填写未提供的量化值。无法执行的检查
必须标记 `NOT RUN`。协议只能描述为“依据自定义 DBC 解析模拟车况信号”，不得暗示
OEM 或量产车辆协议。

## 双开发环境与唯一源码

本项目使用同一个 Git 仓库在两个环境中协作，仓库源码是唯一可信源：

- Windows 主机负责 STM32CubeMX 外设/时钟配置、Keil MDK 编译调试、ST-Link 烧录，
  以及 VS Code + Codex 源码审查和 Git 管理。STM32 的真实编译结论只认 Keil Build
  输出，真实烧录结论只认 Windows + Keil/ST-Link 记录。
- Ubuntu 虚拟机负责 `gatewayd` Linux C 开发、SocketCAN、pthread、有界环形缓冲区
  （bounded ring buffer）、MQTT、epoll/eventfd/timerfd、持久化 spool、ARM 交叉编译、
  BusyBox 部署、自动化/性能测试和 i.MX6ULL 上板调试。
- 两边可以分别 clone 仓库；Windows 主要修改 `stm32/`、`protocol/` 和部分 `docs/`，
  Ubuntu 主要修改 `gateway/`、`tools/`、`deploy/` 和 `docs/`。禁止维护三份互不关联的
  STM32 源码。
- Ubuntu 未安装 `arm-none-eabi-gcc`、OpenOCD 或 STM32CubeMX 不构成 Linux 侧阻塞，
  也不应为此强行搭建另一套 STM32 工具链。

## 已知运行环境

- Linux 目标：100ASK i.MX6ULL、ARMv7、Linux 4.9.88、Buildroot 2020.02、BusyBox
  1.31.1、已有 `can0`，无 systemd。
- 模拟 ECU：STM32F103C8T6，运行时只启用所需 CAN 功能；实际工程和接线采用 bxCAN
  默认引脚 PA11/CAN_RX、PA12/CAN_TX，不启用 CAN remap。
- 物理层：STM32 侧使用带 120 Ω 终端的外置 TJA1050 模块；i.MX6ULL 开发板已配备
  完整 CAN 模块，包含 CAN 控制器、TJA1042T/3 收发器、120 Ω 终端、TVS 保护和
  CANH/CANL 接口。两端终端均已具备，禁止额外并联 120 Ω。
- PC 测试端：局域网内运行 Mosquitto Broker、subscriber/validator 和测试控制程序。
- 基准总线：标准 11-bit Classic CAN、DLC 8、500 kbit/s、总计 111 帧/s。

上述目标板版本与硬件型号来自项目所有者。本轮只是规范更新，未重新连接或测量设备。

## 目标数据链路

```text
STM32F103C8T6 确定性模拟 ECU
  -> STM32 bxCAN（PA11/PA12，默认映射）
  -> STM32 侧 TJA1050 模块（120 Ω）
  -> CANH/CANL/GND 物理总线
  -> i.MX6ULL 板载 CANH/CANL 接口、TVS、120 Ω、TJA1042T/3
  -> i.MX6ULL CAN 控制器和 SocketCAN can0
  -> CAN_RAW_FILTER、recvmsg()、SO_TIMESTAMPNS
  -> 合法性校验和自定义 DBC 解码
  -> 固定大小 telemetry_record
  -> 有界环形缓冲区（bounded ring buffer）
  -> 1 秒 batch 和 append-only spool
  -> 单个 in-flight MQTT QoS 1 发布
  -> PUBACK 后推进确认游标
  -> PC subscriber 和序列校验程序
```

这是最终目标架构，不代表 M0 已实现这些功能。

## STM32 子系统定位和边界

STM32 是确定性 CAN 流量发生器（Deterministic CAN Traffic Generator）/模拟 ECU，
不是本项目的主要技术复杂度来源。它只负责：

- 使用 CubeMX 配置 Clock、CAN、PA11/PA12 默认映射和经过计算的 500 kbit/s bit timing；
- 周期发送 `0x100`、`0x101`、`0x102`；
- 维护各消息 Rolling Counter 和 XOR Checksum；
- 生成便于自动校验的确定性信号，必要时提供 UART 调试和错误统计。

不引入 RTOS、USB 通信协议、GUI、复杂 bootloader、TCP/IP、MQTT 或文件系统。Codex
修改 CubeMX 工程时优先写入 `USER CODE BEGIN/END` 区域，防止重新 Generate Code 时
覆盖业务逻辑。

## 协议基线

| CAN ID | 名称 | 周期 | 速率 | 数据摘要 |
| --- | --- | ---: | ---: | --- |
| `0x100` | VehicleDynamics | 10 ms | 100 帧/s | 车速、转速、油门、挡位、计数器、XOR |
| `0x101` | PowerStatus | 100 ms | 10 帧/s | 电压、水温、SOC、故障位、计数器、XOR |
| `0x102` | BodyStatus | 1000 ms | 1 帧/s | 里程、车门、点火状态、计数器、XOR |

三类消息的 byte 6 分别维护模 256 Rolling Counter，byte 7 是 byte 0～6 的 XOR。
M3-B 依据此冻结基线生成报文；正式 `vehicle.dbc`、黄金向量和 Linux 静态解码器仍在
M4 同步创建和验证。

## Linux 组件边界

- CAN 接收热路径负责 ID 过滤、`SO_TIMESTAMPNS` 辅助数据提取、长度/计数器/校验和
  检查、解码和入队；不得执行 MQTT、持久化同步或复杂序列化。
- `telemetry_record` 使用固定大小结构，保存 gateway 序号、CAN 原始数据、内核时间戳、
  ECU 计数器、解码信号和状态。
- 有界队列维护 capacity/head/tail/count、条件变量、退出唤醒、push/pop/drop 统计和
  high-watermark。计划的满队列策略是短时间有界等待后丢弃新记录。
- MQTT 使用经过验证的 C 库和 QoS 1，初期只允许一个 in-flight batch；只有匹配的
  PUBACK 到达后才算发布成功。
- 持久化使用 append-only `spool.data` 和原子推进的 `spool.state`，包含 CRC、尾部恢复、
  顺序补传和 seq 恢复。
- 只有目标 libmosquitto 实际支持并验证 `mosquitto_socket`、`mosquitto_loop_read`、
  `mosquitto_loop_write`、`mosquitto_loop_misc` 后，M8 才能采用 epoll reactor。
- 部署只使用 BusyBox 兼容 init/respawn/supervisor，不使用 systemd。

## CAN 物理层门禁

STM32 侧 TJA1050 模块和 i.MX6ULL 板载 CAN 模块各有一个 120 Ω，因此禁止未经测量
再并联终端。M3-C 必须在两块开发板及相关模块全部断电时测量 CANH--CANL：约 60 Ω
才符合两只 120 Ω 并联预期；约 120 Ω 可能只有一个终端生效；约 40 Ω 可能存在额外
第三终端。实际值、接线和判断必须进入 M3 证据，测量通过后才能上电。

## 统计、配置和安全边界

最终统计必须区分 CAN 接收/校验/计数器错误、队列流量/丢弃/水位、MQTT
publish/PUBACK/reconnect、spool append/replay/corrupt 和正常退出。ARMv7 上不得未经
验证假定 64-bit 原子操作 lock-free，必须明确同步并测试。

device ID、CAN 接口、Broker 地址/端口/凭据、topic、spool 路径、队列容量和时间参数
必须来自配置或命令行。真实局域网地址和凭据不得提交；初始局域网阶段不宣称 TLS。

## 当前实现边界

M1 已完成 Ubuntu x86_64 主机上的严格配置解析/校验/覆盖、时间戳分级日志和配置脱敏、
稳定错误码、self-pipe 信号生命周期、mutex stats、64 字节 `telemetry_record` 以及
mutex/condition variable 有界环形缓冲区。

M2 已增加 Linux `CAN_RAW` 接收模块：为 `0x100`、`0x101`、`0x102` 安装精确标准数据帧
过滤器，启用 `SO_TIMESTAMPNS`，使用 `poll()` + `recvmsg()` 检查 datagram 长度、DLC、
辅助数据完整性并填充原始 `telemetry_record`。`gatewayd --can-receive COUNT` 只是有总
超时的有限板端验证入口，不向 M1 ring buffer 投递，也不构成 M5 生产者--消费者链路。

M2 已于 2026-08-29 通过：具备 x86_64 warning-clean/单元测试、ASan+UBSan、Buildroot
SDK ARMv7 warning-clean 交叉构建和真实 i.MX6ULL controller loopback 证据。SHA256
固定的 Cortex-A7/ARMv7 hard-float binary 在板端动态加载成功；目标/非目标 ID、错误
DLC 和 `SO_TIMESTAMPNS` 提取均通过最终审计。板端 wall clock 未初始化，因此时间戳
不代表正确 UTC 时间或性能；测试后接口为 DOWN/STOPPED、loopback off，但 500000 bit
timing 在关闭状态保留。

M3-A～M3-D 已由项目所有者按实际现象验收：仓库已有 STM32F103C8T6 CubeMX/Keil 工程，
系统时钟 72 MHz、PCLK1 36 MHz；CAN Prescaler 4、SJW 1 TQ、BS1 13 TQ、BS2 4 TQ，
得到 500 kbit/s 和约 77.78% sample point。Keil Build 为 0 error、0 warning；实际接线
采用 PA11/CAN_RX、PA12/CAN_TX，SoC 侧 `candump` 收到 `0x100`/`0x101`/`0x102`，并由
项目所有者确认周期、DLC、Rolling Counter 和 XOR 正确。上述 STM32/物理 CAN 结果采用
项目所有者简化验收，没有新增完整测试 artifact，不外推为 `gatewayd`、DBC 或可靠性
结果。M3-E 已完成两次1110帧真实 `gatewayd` 短测，均无接收错误；项目所有者取消
原计划的10分钟/按 ID gap 门禁并接受 M3 完成。连续10分钟没有执行，不得写成可靠性
结果。

M4 已于2026-08-31完成。实验用 `vehicle.dbc` 冻结三类消息的 Intel 小端布局与定点
单位；`telemetry_record.decoded_payload` 的32字节语义已定义，并通过 `memcpy` 访问。
STM32 已用整数定点实现60秒语义车况循环，严格按 DBC 编码，spare bits 清零并保留三类
独立 counter/XOR。42条黄金向量和完整6660帧主机模型通过；60秒实物 `candump` 中
`0x100`/`0x101`/`0x102` 分别为6000/600/60帧，逐帧模型差异、counter、XOR、spare
bit 差异和 CAN 错误计数增量均为0。项目所有者确认 Keil Build 0 error/0 warning 且
已 Download，但完整原始 Keil 输出未归档。M4 关闭时新增 ARM binary 尚未部署或板端
运行；该历史缺口已在后续 M5 目标板测试中补齐。

M5 已实现一个 CAN producer 和一个 mock-sink consumer：producer 以100 ms有限 poll
调用 M2 receiver，通过 M4 checksum/DBC 解码后按 M1 配置有界入队；满队列有界等待后
丢弃新记录。consumer 在 close 后先 drain，并统计消费、seq gap、非单调和无效记录。
`SIGINT`/`SIGTERM` 通过 self-pipe 唤醒主线程，再 close/broadcast 和 join 工作线程。
Ubuntu 主机 warning-clean/ASan+UBSan、111帧/s合成基准、故意过载、SIGTERM及 ARMv7
交叉构建已经通过。项目所有者随后在真实 i.MX6ULL 上运行 SHA256 固定的 ARM binary：
基准物理输入3694条全部 decode/入队/消费且 queue drop 为0；容量4、零等待、20 ms慢
consumer 的过载测试按策略记录3561次 queue drop；两次均在 signal 15 后输出线程 join
后的 summary。M5 已于2026-08-31通过。STM32 在进程启动后才中途开启，因此305/75次
receive timeout 只表示此前无输入时的100 ms poll；不能把整段64/69秒写成持续111帧/s。
本次没有 `can_before/after`、正确 UTC 或 shell `wait` 精确退出码，故不产生 CAN 错误
增量、持续运行、吞吐、时延或可靠性结论。

M6 使用libmosquitto MQTT QoS 1 sink，按单调时钟和固定上限组成JSON batch，显式限制
单个in-flight publish，只有收到匹配MID的PUBACK才推进确认统计。除Ubuntu loopback
1000-batch外，项目所有者已在真实i.MX6ULL上使用私有ARMv7 libmosquitto 2.0.11和
物理CAN输入连接Windows Mosquitto 2.1.2；正式subscriber保存1000批/115335条连续记录，
gateway与Broker对账1033次publish/PUBACK一致。独立manifest和原始证据复核已通过，
M6于2026-09-01达到门禁。正确UTC、性能、时延和长期可靠性仍未验证。

M7已用真实Windows Broker、i.MX6ULL ext4 spool和一次受控`kill -9`完成断线补传及
tail/internal/state损坏恢复；去重后35644条unique seq无缺失，门禁为`MET`。M8已确认
目标libmosquitto 2.0.11具备所需external-loop API，并实现epoll/eventfd/timerfd reactor；
Ubuntu/ARM和真实Windows Broker+i.MX6ULL恢复门禁通过，去重后27434条seq连续，M8为
`MET`。M9已实现BusyBox inittab/前台supervisor并通过主机、sanitizer和ARM构建验证；
Windows续跑已从真实目标只读确认PID 1链接`libbusybox.so.1.31.1`，但指定M9 binary
无法从Ubuntu认证转交且不在Windows/目标板上，故未重新计算部署输入SHA，也未执行
staging、`/etc`安装、开机和异常拉起；这些仍为`NOT RUN`，M9总门禁为`NOT MET`。
M10尚未开始。
