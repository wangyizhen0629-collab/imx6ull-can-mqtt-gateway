# 分阶段实施计划

同一时间只能有一个活动 Milestone。只有退出条件具备真实证据时才能通过；无法执行的
门禁必须写成 `NOT RUN`，不能用推测代替。进入下一阶段需要用户另行确认。

## 当前状态

- M0 已于 2026-08-28 通过，证据保持不变。
- M1 已于 2026-08-28 通过；最终 warning-clean 主机 run 和 ASan+UBSan run 均为
  8/8 通过。LeakSanitizer 因当前执行环境处于 `ptrace` 下标记为 `NOT RUN`。
- M2 已于 2026-08-29 通过。SocketCAN 源码、warning-clean x86_64 与 ARMv7 构建、
  主机普通/ASan+UBSan 9/9、真实 i.MX6ULL 动态加载和 controller loopback 均有证据。
  板端目标 ID、非目标过滤、DLC 拒绝及内核 timestamp 用例全部 PASS。
- 本轮已在 M2 完成后停止。M3-A 及后续阶段仍为“未开始”，没有因 M2 通过自动进入；
  开始下一阶段需要用户另行确认。

## Milestone 总表

| Milestone | 范围 | 退出门禁 | 状态 |
| --- | --- | --- | --- |
| M0 | 环境审计、架构文档、仓库与主机骨架 | 文档齐全；主机 `gatewayd` 可配置、编译、运行；未知项和 `NOT RUN` 已记录 | 2026-08-28 已通过 |
| M1 | 配置、日志、错误、生命周期、stats、记录类型、有界环形缓冲区 | 主机单元测试覆盖正常、满队列、退出唤醒和并发行为 | 2026-08-28 已通过 |
| M2 | i.MX6ULL SocketCAN loopback 接收、过滤、时间戳 | ARM 交叉编译和真实板端日志证明目标/非目标 ID 过滤及时间戳提取 | 2026-08-29 已通过 |
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

## M2 完成记录

实现和退出证据如下：

1. `CAN_RAW` socket 安装 `0x100`、`0x101`、`0x102` 三条精确标准数据帧过滤器，mask
   同时区分 EFF/RTR；默认不订阅 CAN error frame。
2. 启用 `SO_TIMESTAMPNS`，通过 `poll()` + `recvmsg()` 提取内核时间戳，拒绝短/长
   datagram、非目标/带 flag 的 ID、DLC 非 8、控制消息截断和缺失/非法时间戳。
3. `gatewayd --can-receive COUNT --can-timeout-ms MS` 提供有限帧数、总超时的板端门禁
   入口；只记录原始帧和 M2 stats，不接入 ring buffer、DBC、MQTT 或其他 M3+ 功能。
4. 初始最终主机 run `artifacts/20260829T092323+0800-m2-host-final/` warning-clean 构建
   及 CTest 9/9 PASS；`test_can_receiver` 实际覆盖未绑定 CAN_RAW socket 选项、主机
   内核 timestamp 和错误注入。为目标 glibc 增加 `_DEFAULT_SOURCE` 后，又以
   `artifacts/20260829T131536+0800-m2-host-regression/` 完成 warning-clean 9/9 回归，
   并以 `artifacts/20260829T131705+0800-m2-asan-ubsan-regression/` 完成 ASan+UBSan
   9/9 回归；LSan 仍为 `NOT RUN`。
5. 真实板端只读审计 `artifacts/19700101T123711+0000-m2-board-audit/` 确认 i.MX6ULL、
   ARMv7、Linux 4.9.88、Buildroot 2020.02-g65177d4、glibc 2.30 loader、FlexCAN
   `can0`、`candump`/`cansend` 和 245 MiB 可用 `/tmp`。板端时钟未初始化而报告 1970；
   原 run_id 保留，不能作为真实日期。
6. 用户提供并已 relocate 的 SDK 位于被 `.gitignore` 排除的 `ToolChain/`。仓库内
   `gateway/cmake/toolchains/imx6ull-buildroot.cmake` 从环境变量读取 SDK 根目录；最终
   ARM run `artifacts/20260829T131442+0800-m2-arm-cross-final/` 使用 GCC 7.5.0、
   `arm-buildroot-linux-gnueabihf`、glibc 2.30 sysroot warning-clean 构建成功。输出为
   Cortex-A7/ARMv7 EABI5 hard-float ELF32，解释器 `/lib/ld-linux-armhf.so.3`，SHA256
   为 `be27554bafac535e45908e881117a185965470f21ae9645f2fcb0ca0a1ba5595`。
7. 板端部署核验 `artifacts/20260829T132938+0800-m2-board-deploy-verify/` 确认
   `/tmp/gatewayd-m2` SHA256 与交叉构建产物完全一致；紧邻测试前 `can0` 仍为
   DOWN/STOPPED，流量和错误计数均为 0。本 run 没有启动程序或修改接口。
8. 经明确批准的真实板端 run `artifacts/20260829T133148+0800-m2-board-loopback/`
   动态加载 PASS；按顺序接收 `0x100`/`0x101`/`0x102` 三帧及正数内核时间戳；只发送
   `0x123` 时零接收并预期 timeout；`0x100` DLC 3 被拒绝并预期 timeout。8 个发送帧
   共 59 字节，TX/CAN error 为 0。
9. 最终审计 `artifacts/20260829T134148+0800-m2-final-audit/` 对上传 tar 的 18 个原始
   成员逐字节复核，并重新检查 binary、日志和状态，全部 PASS。第一次把 UID/GID 差异
   当成内容差异的审计 `artifacts/20260829T134053+0800-m2-final-audit/` 保留为 FAIL，
   没有覆盖。

退出判定和限制：

- ARM 交叉编译、板端动态加载、controller loopback、目标/非目标 ID、错误 DLC 和
  `SO_TIMESTAMPNS` 提取均满足 M2 退出条件。
- 板端 wall clock 未初始化；时间戳只证明提取和递增顺序，不能描述为正确 UTC 时间、
  CAN 时延或性能。
- 测试后 `can0` 为 DOWN/STOPPED 且 loopback off；500000 bit timing 按执行前说明仍在
  DOWN 状态保留，不能描述为完全恢复未配置状态。
- 物理 CAN、STM32、DBC、producer--consumer、MQTT 及性能均不属于 M2，仍为
  `NOT RUN`。

因此 M2 状态为“已通过”，详见 `docs/milestones/M2.md`。controller loopback 结果不得
描述成物理 CAN 或 STM32 结果。

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

## M1 完成记录

M1 是 Ubuntu 主机侧阶段，未触碰真实 CAN 或 MQTT，也不依赖 STM32 Linux 工具链：

1. 冻结配置 schema、优先级、校验和敏感信息脱敏规则，覆盖有效/无效/边界测试。
2. 增加带时间戳和等级的日志、稳定错误码、信号驱动生命周期和确定性退出测试。
3. 定义固定大小 `telemetry_record`，增加适合 ARMv7 的大小/字段布局检查。
4. 使用 mutex/condition variable 实现 stats 和有界环形缓冲区，包括 `while` 条件等待、
   producer 有界超时、close/broadcast、计数器和 high-watermark。
5. 添加无外部测试框架的 FIFO、满队列、超时、退出唤醒和多线程不变量测试。
6. 执行 warning-clean 主机构建和环境支持的 sanitizer；不同测试使用独立 run_id，且
   主机结果不得表述为板端结果。

上述 1～6 已完成。最终普通主机证据为
`artifacts/20260828T234222+0800-m1-host-final/`，ASan+UBSan 证据为
`artifacts/20260828T234154+0800-m1-asan-ubsan/`，完整过程和 `NOT RUN` 项见
`docs/milestones/M1.md`。M1 通过本身不能证明 M2、ARMv7 或板端行为。
