# 分阶段实施计划

同一时间只能有一个活动 Milestone。只有退出条件具备真实证据时才能通过；无法执行的
门禁必须写成 `NOT RUN`，不能用推测代替。进入下一阶段需要用户另行确认。

## 当前状态

- M0 已于 2026-08-28 通过，证据保持不变。
- 本次仅完成架构规范调整，不算开始 M1，也不产生新的功能测试结论。
- 当前没有活动 Milestone；项目处于“**M0 已完成，等待用户确认是否启动 M1**”状态。

## Milestone 总表

| Milestone | 范围 | 退出门禁 | 状态 |
| --- | --- | --- | --- |
| M0 | 环境审计、架构文档、仓库与主机骨架 | 文档齐全；主机 `gatewayd` 可配置、编译、运行；未知项和 `NOT RUN` 已记录 | 2026-08-28 已通过 |
| M1 | 配置、日志、错误、生命周期、stats、记录类型、有界环形缓冲区 | 主机单元测试覆盖正常、满队列、退出唤醒和并发行为 | 未开始 |
| M2 | i.MX6ULL SocketCAN loopback 接收、过滤、时间戳 | ARM 交叉编译和真实板端日志证明目标/非目标 ID 过滤及时间戳提取 | 未开始 |
| M3-A | Windows CubeMX 基础工程 | `.ioc` 已保存；Keil 工程生成成功；Keil Build 成功 | 未开始 |
| M3-B | Windows STM32 确定性模拟 ECU | 三类周期报文、Rolling Counter、XOR 和确定性数据实现；Keil Build 成功 | 未开始 |
| M3-C | 断电物理层检查 | 接线/模块记录完整；CANH--CANL 实测接近 60 Ω 并有证据 | 未开始 |
| M3-D | i.MX6ULL 物理 CAN 与 `candump` | 经批准烧录 STM32、关闭 loopback；`candump` 看到三类 ID、正确周期和 Rolling Counter | 未开始 |
| M3-E | `gatewayd` 接入真实 CAN | 只在 M3-D 通过后切换；连续 10 分钟接收，CAN 状态和计数器结果可解释 | 未开始 |
| M4 | 自定义 DBC、静态解码器、黄金向量 | 主机黄金向量通过，真实 STM32 规律与解码结果一致 | 未开始 |
| M5 | 生产者--消费者链路和 mock sink | 基准负载 queue drop 为 0；过载策略和 SIGTERM 退出通过 | 未开始 |
| M6 | libmosquitto MQTT QoS 1 基线 | 实际库已验证；至少 1000 batch，seq 和 PUBACK 统计通过 | 未开始 |
| M7 | 持久化 spool、恢复、重连补传 | 断线/恢复和 `kill -9` 证明顺序恢复及去重后完整 | 未开始 |
| M8 | 条件式 epoll network reactor | 外部 loop API 可用且保持 M7 行为，否则记录删除 epoll 的决定 | 未开始 |
| M9 | BusyBox 部署与进程恢复 | 开机启动和异常拉起通过，不使用 systemd | 未开始 |
| M10 | 自动化中断、性能和 24 小时证据 | 压力、断网、`/proc` 指标、稳定性和简历追溯报告齐全 | 未开始 |

## M3-A～M3-E 顺序门禁

五个子阶段必须按顺序执行，不得把其中任一项的结果外推到后续阶段：

### M3-A：STM32 CubeMX 基础工程（Windows）

1. 用 STM32CubeMX 创建 STM32F103C8T6 工程。
2. 记录 Clock Tree、APB1 时钟、CAN Remap、PB8/CAN_RX、PB9/CAN_TX。
3. 根据真实时钟计算并记录 500 kbit/s bit timing、sample point 和 SJW。
4. Generate Code 生成 Keil 工程并执行 Keil Build。
5. 保存 `.ioc`、`.uvprojx` 和 Keil Build 输出；`Objects/`、`Listings/` 等中间文件不提交。

Codex 若无法执行 Windows Keil，必须写：`NOT RUN - 需要用户在 Windows Keil 中验证`。

### M3-B：STM32 模拟 ECU（Windows）

1. 优先在 CubeMX `USER CODE BEGIN/END` 区域实现业务逻辑。
2. 按冻结协议发送 `0x100` 100 Hz、`0x101` 10 Hz、`0x102` 1 Hz。
3. 实现独立 Rolling Counter、byte 0～6 XOR Checksum 和确定性信号变化。
4. 必要时增加简洁 UART 调试和错误计数，不引入 RTOS、USB 协议或网络栈。
5. 以 Keil Build 输出作为编译门禁；未执行时不得声称成功。

### M3-C：CAN 物理层检查（断电）

1. 两块开发板、STM32 侧 TJA1050 模块和 i.MX6ULL 板载 CAN 模块全部断电。
2. 核对 STM32 PB8/PB9 与 TJA1050 TXD/RXD、两端 CANH/CANL/GND、STM32 侧模块
   电源和逻辑电平；i.MX6ULL 侧直接使用板载 CANH/CANL 接口。
3. 禁止额外并联 120 Ω；测量 CANH--CANL 等效电阻。
4. 预期约 60 Ω；约 120 Ω 或约 40 Ω 时停止上电并排查终端数量。
5. 保存实测值、仪表/接线记录和判定。

### M3-D：i.MX6ULL 物理 CAN

M3-C 通过后，先获得 STM32 烧录批准，在 Windows 使用 Keil/ST-Link 下载 M3-B 固件并
保存真实烧录结果。随后必须再次获得修改 `can0` 状态的用户批准，再执行：

```sh
ip link set can0 down
ip link set can0 type can bitrate 500000 loopback off
ip link set can0 up
candump can0
```

先用 `candump` 验证 `0x100`、`0x101`、`0x102`，再验证周期、Rolling Counter、
Checksum 和 CAN error/state；不得跳过烧录记录或 `candump`。

### M3-E：gatewayd 接入真实 CAN

只有“STM32 -> TJA1050 -> CANH/CANL/GND -> i.MX6ULL 板载 TJA1042T/3 CAN 模块
-> SocketCAN -> candump”链路已经通过，才允许 `gatewayd` 切换到真实 CAN 输入。
最终保存至少 10 分钟连续接收、CAN 状态、每 ID Rolling Counter gap 和错误统计；
未产生的数值不得预填。

## M1 建议任务

M1 仍为 Ubuntu 主机侧阶段，不触碰真实 CAN 或 MQTT，也不依赖 STM32 Linux 工具链：

1. 冻结配置 schema、优先级、校验和敏感信息脱敏规则，覆盖有效/无效/边界测试。
2. 增加带时间戳和等级的日志、稳定错误码、信号驱动生命周期和确定性退出测试。
3. 定义固定大小 `telemetry_record`，增加适合 ARMv7 的大小/字段布局检查。
4. 使用 mutex/condition variable 实现 stats 和有界环形缓冲区，包括 `while` 条件等待、
   producer 有界超时、close/broadcast、计数器和 high-watermark。
5. 添加无外部测试框架的 FIFO、满队列、超时、退出唤醒和多线程不变量测试。
6. 执行 warning-clean 主机构建和环境支持的 sanitizer；不同测试使用独立 run_id，且
   主机结果不得表述为板端结果。
