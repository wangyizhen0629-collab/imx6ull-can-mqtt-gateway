# 简历事实可追溯性

当前仍不能写“完整 CAN--MQTT 网关”、物理 CAN、STM32 模拟 ECU、DBC、可靠性或性能
结论。M1 已证明
x86_64 主机上的配置和并发基础设施单元行为；M2 又完成 ARMv7 warning-clean 交叉构建，
并在真实 i.MX6ULL controller loopback 上证明当前 `gatewayd` 的 SocketCAN 精确过滤、
DLC 拒绝和内核时间戳提取。只允许使用下表明确标为“是”的窄范围表述。

| 候选描述 | 必需源码/配置 | 必需测试 | 必需证据 | 真实值 | 可写简历 |
| --- | --- | --- | --- | --- | --- |
| SocketCAN 接收/过滤/内核时间戳和自定义 DBC 解码 | M2/M4 源码和协议文件 | loopback、过滤、时间戳、黄金/实时报文解码 | M2 主机/ARM/板端 loopback 已通过；M3-D/E、M4 未运行 | M2 controller loopback 已验证；无物理 CAN/DBC 值 | 否 |
| 在 i.MX6ULL 上实现 CAN_RAW 精确 ID 过滤、DLC 校验和 SO_TIMESTAMPNS 提取，并以 controller loopback 验证 | M2 `can_receiver`、有限 CLI、toolchain file | 主机错误注入、ARM 构建、真实板端目标/非目标/DLC/timestamp | M2 最终主机、ARM、板端和审计 run | 目标 3/3；非目标 0；DLC reject 1；timestamp 3/3；仅 controller loopback | 是，必须保留 loopback 限定且不得写性能 |
| STM32 确定性模拟 ECU 提供真实物理 CAN 输入 | M3-A/B `.ioc`、Keil 工程和业务源码 | Keil Build、断电终端测量、`candump`、周期/计数器 | M3 preflight 仅确认 M2 前置门禁；M3-A～M3-D 均未通过 | NOT RUN | 否 |
| pthread 有界生产者--消费者队列及明确过载策略 | M1/M5 队列、生命周期、stats 配置 | 并发/满队列/close 单测和目标基准/过载 | M1 主机 8/8 已通过；M5 板端未运行 | M1 功能测试：3 producer × 2000 条全部唯一消费；非性能值 | 否 |
| MQTT QoS 1、seq、PUBACK、本地 spool、重连补传 | M6/M7 源码和 spool 格式/配置 | 1000 batch、断线、损坏、崩溃恢复 | M6/M7 集成 run | 未测量 | 否 |
| epoll 统一 eventfd/timerfd/MQTT socket | M8 reactor 和 API 兼容性记录 | 与 M7 等价的 reactor/重连/退出测试 | M8 目标 run | 未测量 | 否 |
| BusyBox 开机启动和异常退出恢复 | M9 init/supervisor/config | 启动和受控 crash/restart | M9 板端 run | 未测量 | 否 |
| 压力、重复断网、CPU/RSS 和 24 小时稳定性 | M10 工具和精确配置 | 经批准的压力/断网/稳定性流程 | M10 报告 | 未测量 | 否 |

一行满足条件后，只能用已有不可变 run 计算出的数值替换“未测量”，并补充精确源码、
测试、配置和 `artifacts/<run_id>/` 链接。STM32 Keil 结果与 Linux 结果必须分别标明环境。

M1 可追溯证据为 `artifacts/20260828T234222+0800-m1-host-final/` 和
`artifacts/20260828T234154+0800-m1-asan-ubsan/`。其中 6000 条记录仅是无并发重复/遗漏
的单元测试规模，未测量吞吐率、时延、CPU、内存或持续运行时间，不能转换成性能描述。

M2 最新主机可追溯证据为 `artifacts/20260829T131536+0800-m2-host-regression/` 和
`artifacts/20260829T131705+0800-m2-asan-ubsan-regression/`；ARM 构建为
`artifacts/20260829T131442+0800-m2-arm-cross-final/`；真实板端只读审计为
`artifacts/19700101T123711+0000-m2-board-audit/`。主机 run 只证明 x86_64 上未绑定
CAN_RAW socket 过滤选项、内核 datagram timestamp 解析及错误路径；ARM run 只证明
交叉构建和离线 ABI；板端审计只证明板型、系统、`can0` 和工具前置条件。部署 checksum
核验为 `artifacts/20260829T132938+0800-m2-board-deploy-verify/`，只证明板端文件与构建
产物一致。真实 i.MX6ULL controller loopback 为
`artifacts/20260829T133148+0800-m2-board-loopback/`，最终逐字节审计为
`artifacts/20260829T134148+0800-m2-final-audit/`。因此仅第二行的 M2 窄范围描述可用；
第一行仍受 M3-D/E 和 M4 阻塞。板端时钟未初始化，三个正数 timestamp 不能表述为 CAN
时延、UTC 正确性或性能。

M3 preflight 为 `artifacts/20260829T141730+0800-m3-preflight/`。它只证明 M2 前置门禁
经再次核验、当前仓库缺少 `.ioc`/`.uvprojx` 且 Ubuntu 没有 CubeMX/Keil；不是 STM32
源码、Keil Build、硬件测量、烧录、物理 `candump` 或 10 分钟接收证据。因此 M3 相关
简历描述仍不可用。
