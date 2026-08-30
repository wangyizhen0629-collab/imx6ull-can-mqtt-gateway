# 分阶段实施计划

同一时间只能有一个活动 Milestone。只有退出条件具备真实依据时才能通过；项目所有者
选择简化验收时必须明确记录未归档原始日志，不能用推测补齐数值。无法执行的门禁必须
写成 `NOT RUN`。进入下一阶段需要用户另行确认。

## 当前状态

- M0 已于 2026-08-28 通过，证据保持不变。
- M1 已于 2026-08-28 通过；最终 warning-clean 主机 run 和 ASan+UBSan run 均为
  8/8 通过。LeakSanitizer 因当前执行环境处于 `ptrace` 下标记为 `NOT RUN`。
- M2 已于 2026-08-29 通过。SocketCAN 源码、warning-clean x86_64 与 ARMv7 构建、
  主机普通/ASan+UBSan 9/9、真实 i.MX6ULL 动态加载和 controller loopback 均有证据。
  板端目标 ID、非目标过滤、DLC 拒绝及内核 timestamp 用例全部 PASS。
- M3-A～M3-D 已由项目所有者按简化的实际现象口径验收：实际 MCU/8 MHz 晶振已确认，
  STM32 工程改用 PA11/CAN_RX、PA12/CAN_TX 默认映射；Keil Build 为 0 error、0 warning；
  接线和终端已核对，物理 `candump` 已确认三类 ID、周期、DLC、独立 Rolling Counter
  和 XOR 正确。项目所有者明确不补建这些 STM32 侧 artifact，因此状态必须同时保留
  “用户确认、未归档原始日志”的限定。
- `gatewayd` 已在真实 `can0` 完成两次1110帧短时接收，summary 均为 accepted=1110，
  timeout/reject/timestamp/receive error 全为0；干净复测期间 CAN 状态和错误计数无新增
  异常。项目所有者决定不再执行原计划的10分钟 M3-E 测试，也不增加长时/按 ID gap
  统计，并接受 M3 完成。该决定不等价于10分钟或可靠性 PASS。
- M4 已于 2026-08-30 完成首轮 `vehicle.dbc`、20条向量和32字节定点解码布局；默认
  warning-clean 与 ASan+UBSan 全量回归均11/11 PASS，ARMv7 warning-clean 交叉构建
  PASS。随后项目所有者明确：STM32 必须生成有物理意义的模拟车速、转速等信号并按
  DBC 编码。现有全部768种 counter 测试只证明旧原始字节递增规律可解码，不能满足该
  语义要求。因此 M4 门禁当前为 **NOT MET**，等待 Windows 修正固件、Keil/`candump`
  依据和新的物理场景黄金向量；已有 artifact 保持历史结果不变。M5 及后续未开始。

## Milestone 总表

| Milestone | 范围 | 退出门禁 | 状态 |
| --- | --- | --- | --- |
| M0 | 环境审计、架构文档、仓库与主机骨架 | 文档齐全；主机 `gatewayd` 可配置、编译、运行；未知项和 `NOT RUN` 已记录 | 2026-08-28 已通过 |
| M1 | 配置、日志、错误、生命周期、stats、记录类型、有界环形缓冲区 | 主机单元测试覆盖正常、满队列、退出唤醒和并发行为 | 2026-08-28 已通过 |
| M2 | i.MX6ULL SocketCAN loopback 接收、过滤、时间戳 | ARM 交叉编译和真实板端日志证明目标/非目标 ID 过滤及时间戳提取 | 2026-08-29 已通过 |
| M3-A | Windows CubeMX 基础工程 | `.ioc` 已保存；Keil 工程生成成功；Keil Build 成功 | 已完成；项目所有者确认 Build 0 error/0 warning，采用 PA11/PA12 默认映射 |
| M3-B | Windows STM32 确定性模拟 ECU | 三类周期报文、Rolling Counter、XOR 和确定性数据实现；Keil Build 成功 | 历史字节规律已验收；M4 澄清所需语义化物理信号编码待 Windows 修正 |
| M3-C | 断电物理层检查 | 接线/模块记录完整；CANH--CANL 终端检查合理 | 已完成；项目所有者确认接线和终端已核对，具体欧姆值未归档 |
| M3-D | i.MX6ULL 物理 CAN 与 `candump` | 烧录 STM32、关闭 loopback；`candump` 看到三类 ID、正确周期和 Rolling Counter | 已完成；项目所有者确认物理 `candump` 及10分钟运行现象正常 |
| M3-E | `gatewayd` 接入真实 CAN | 在真实 `can0` 完成有限接收；原10分钟门禁由项目所有者豁免 | 已完成；两次1110帧 smoke 正常，10分钟测试 NOT RUN/已豁免 |
| M4 | 自定义 DBC、静态解码器、黄金向量 | 主机黄金向量通过，有物理意义的真实 STM32 模拟规律与解码结果一致 | 进行中；首轮静态测试 PASS，但等待 STM32 按 DBC 语义化编码和复测 |
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
2. 记录 Clock Tree、APB1 时钟、PA11/CAN_RX、PA12/CAN_TX 默认映射。
3. 根据真实时钟计算并记录 500 kbit/s bit timing、sample point 和 SJW。
4. Generate Code 生成 Keil 工程并执行 Keil Build。
5. 保存 `.ioc`、`.uvprojx` 和 Keil Build 输出；`Objects/`、`Listings/` 等中间文件不提交。

Codex 若无法执行 Windows Keil，必须写：`NOT RUN - 需要用户在 Windows Keil 中验证`。

历史 Windows preflight `artifacts/20260829T144926+0800-m3a-preflight-windows/` 的
BLOCKED/NOT RUN 记录保持不变。此后项目所有者提供实物信息并完成工程：MCU 为
STM32F103C8T6，外部晶振 8 MHz，SYSCLK 72 MHz、PCLK1 36 MHz；CAN 采用 PA11/PA12
默认映射，Prescaler 4、SJW 1 TQ、BS1 13 TQ、BS2 4 TQ，实际 500 kbit/s，sample point
约 77.78%。项目所有者确认 Keil Build 为 0 error、0 warning，M3-A 已完成。

### M3-B：STM32 模拟 ECU（Windows）

1. 优先在 CubeMX `USER CODE BEGIN/END` 区域实现业务逻辑。
2. 按冻结协议发送 `0x100` 100 Hz、`0x101` 10 Hz、`0x102` 1 Hz。
3. 实现独立 Rolling Counter、byte 0～6 XOR Checksum 和确定性信号变化。
4. 必要时增加简洁 UART 调试和错误计数，不引入 RTOS、USB 协议或网络栈。
5. 以 Keil Build 输出作为编译门禁；未执行时不得声称成功。

项目所有者确认上述实现已经实际运行，SoC 端看到 `0x100`/`0x101`/`0x102`，周期约
10/100/1000 ms、DLC 8、byte 6 各自递增且 byte 7 XOR 正确，因此 M3-B 已完成。

### M3-C：CAN 物理层检查（断电）

1. 两块开发板、STM32 侧 TJA1050 模块和 i.MX6ULL 板载 CAN 模块全部断电。
2. 核对 STM32 PA11/PA12 与 TJA1050 RXD/TXD、两端 CANH/CANL/GND、STM32 侧模块
   电源和逻辑电平；i.MX6ULL 侧直接使用板载 CANH/CANL 接口。
3. 禁止额外并联 120 Ω；测量 CANH--CANL 等效电阻。
4. 预期约 60 Ω；约 120 Ω 或约 40 Ω 时停止上电并排查终端数量。
5. 保存实测值、仪表/接线记录和判定；本项目当前按项目所有者的简化验收记录为
   “接线及终端检查正常”，未归档具体欧姆值。

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

项目所有者已完成烧录和物理 `candump`，并确认三类 ID、周期、DLC、counter、XOR 及
10分钟运行现象正常。该10分钟结果是物理 CAN/`candump` 验收，不是 M3-E `gatewayd`
结果。

### M3-E：gatewayd 接入真实 CAN

只有“STM32 -> TJA1050 -> CANH/CANL/GND -> i.MX6ULL 板载 TJA1042T/3 CAN 模块
-> SocketCAN -> candump”链路已经通过，才允许 `gatewayd` 切换到真实 CAN 输入。
原计划要求至少10分钟连续接收、按 ID Rolling Counter gap 和 CAN error 统计。实际已
完成两次1110帧短时真实 CAN smoke test：`accepted=1110`，所有 timeout/reject/
timestamp/receive error 为0；项目所有者确认干净复测的 CAN 状态和错误计数无新增异常。

当前 binary 的 `--can-timeout-ms` 上限仍为60000 ms，summary 仍没有按 ID gap，因此没有
执行也不能声称完成10分钟测试。2026-08-30，项目所有者明确取消该剩余测试和代码扩展，
接受 M3 以现有短时结果关闭。M3 状态据此为“已完成（10分钟门禁已豁免）”，而不是
“10分钟测试 PASS”；不得将这一决定外推成可靠性或性能结论。

## M4 首轮实现与待完成门禁

1. `protocol/vehicle.dbc` 冻结三类 DLC-8 标准帧的 Intel 小端位布局、缩放、偏移和单位；
   byte 6 为各消息独立的 `RollingCounter`，byte 7 为 byte 0～6 XOR。该协议明确是实验
   用自定义协议，不是 OEM 或量产车辆协议。
2. `gateway_vehicle_decode_frame()` 是无动态分配的静态解码器；物理量使用明确的整数
   定点单位。`gateway_vehicle_decode_record()` 通过 `memcpy` 写入原有 32 字节
   `decoded_payload`，成功时设置 checksum/decode 状态位，失败时清零解码区并保留其他
   原始记录状态。M4 没有把该调用接入生产者--消费者线程。
3. `protocol/test_vectors/vehicle_golden.csv` 保存20条首轮人工向量，覆盖三类消息、
   小端、缩放/偏移、bit mask、最小/最大值、counter 0/1/255、checksum mismatch、
   未知 ID 和错误 DLC。C 单测和独立 Python 标准库 DBC 检查器读取同一文件。
4. `test_vehicle_decoder` 还按照旧 M3 固件的三个 base 和
   `(base + counter + byte_index) mod 256` 规则遍历全部768帧，解码后的信号、counter
   和 XOR 均通过。M3 中项目所有者已确认真实 `candump` 遵循同一规律；这只建立静态
   规律对应关系，不等价于本轮执行了新的硬件解码。
5. 最终主机证据 `artifacts/20260830T205736+0800-m4-host-final/` 为默认配置
   warning-clean build 和 CTest 11/11 PASS；其中 M4 标签2/2 PASS。ASan+UBSan 证据
   `artifacts/20260830T205834+0800-m4-asan-ubsan/` 也是11/11 PASS；LeakSanitizer 因
   已知 `ptrace` 限制为 `NOT RUN`。
6. ARM 证据 `artifacts/20260830T205937+0800-m4-arm-cross/` 使用 Buildroot GCC 7.5.0
   warning-clean 构建成功；`gatewayd.armv7` 是 ARM EABI5 hard-float ELF32，解释器为
   `/lib/ld-linux-armhf.so.3`，SHA256 为
   `2f0c4680ddc1a4c39de4782515bae227e0d90ea47815e5e790c5966ba0425ab1`。本轮没有部署或
   执行该 binary。
7. 过程证据没有覆盖：首个开发 run 的表头断言失败和受限沙箱 PF_CAN 失败、修正后的
   M4-only PASS，以及 Release/FORTIFY 暴露既有 M1 警告的 FAIL run 均保持原样。Release
   失败不改写成 PASS，也没有在 M4 越界修改 `lifecycle.c`/`log.c`。

上述结果只满足首轮静态实现。项目所有者随后明确“STM32 在不接传感器时也必须生成
有物理意义的模拟车速、转速等信号，并按同一 DBC 编码”，因此 M4 退出门禁当前为
**NOT MET**。关闭前还需要：Windows 侧使用整数定点物理场景生成器替换旧 byte 递增
算法；提供真实 Keil Build 和 `candump` 依据；更新/扩展黄金向量并在新 run 中复测 DBC
与 C 解码器。真实 i.MX6ULL 物理 CAN 上运行新增解码器仍为 **NOT RUN**。M5 及后续
功能未实现，进入下一阶段仍需用户另行确认。

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
