# 简历事实可追溯性

当前仍不能写“完整 CAN--MQTT 网关”、可靠性或性能结论。M1 已证明
x86_64 主机上的配置和并发基础设施单元行为；M2 又完成 ARMv7 warning-clean 交叉构建，
并在真实 i.MX6ULL controller loopback 上证明当前 `gatewayd` 的 SocketCAN 精确过滤、
DLC 拒绝和内核时间戳提取；M4 又完成自定义 DBC、静态 C 解码器、语义化 STM32
模拟信号和实物 CAN 输入一致性闭环。M5 又在真实 i.MX6ULL 上完成物理 CAN → 实时
DBC 解码 → 有界队列 → mock sink 的基准、故意过载和 signal 15 退出验证。只允许使用
下表明确标为“是”的窄范围表述。

| 候选描述 | 必需源码/配置 | 必需测试 | 必需证据 | 真实值 | 可写简历 |
| --- | --- | --- | --- | --- | --- |
| 为实验用自定义 DBC 实现无动态分配的静态 C 解码器 | M4 `vehicle.dbc`、黄金向量、`vehicle_decoder` | DBC/向量交叉检查、边界/错误、语义化 STM32 规律、ARM 构建 | 历史主机/ASan/ARM与当前语义主机/实物CAN证据 | 3类消息；42条向量；6660帧主机模型和实物捕获均匹配 | 是，必须注明自定义协议和离线静态解码 |
| SocketCAN 接收/过滤/内核时间戳与自定义 DBC 解码已在 i.MX6ULL 实时 mock-sink 链路集成 | M2/M4/M5 源码和协议文件 | loopback、过滤、时间戳、黄金向量及板端实时解码 | M2/M4 已通过；M5 主机/ARM及板端物理 CAN 基准/过载通过 | 基准3694条 decode/消费、queue drop 0；仅 mock sink，不含 MQTT、性能或可靠性 | 是，必须注明自定义协议、mock sink 和功能验证限定 |
| 在 i.MX6ULL 上实现 CAN_RAW 精确 ID 过滤、DLC 校验和 SO_TIMESTAMPNS 提取，并以 controller loopback 验证 | M2 `can_receiver`、有限 CLI、toolchain file | 主机错误注入、ARM 构建、真实板端目标/非目标/DLC/timestamp | M2 最终主机、ARM、板端和审计 run | 目标 3/3；非目标 0；DLC reject 1；timestamp 3/3；仅 controller loopback | 是，必须保留 loopback 限定且不得写性能 |
| STM32 确定性模拟 ECU 提供真实物理 CAN 输入 | M3-A/B `.ioc`、Keil 工程和业务源码 | Keil Build、接线/终端检查、`candump`、周期/计数器 | 项目所有者简化验收；原始日志未归档 | PA11/PA12、500 kbit/s；三类 ID/周期/DLC/counter/XOR 正常 | 是，但必须注明为项目实测且不写可靠性/性能数字 |
| pthread 有界生产者--消费者队列及明确过载策略 | M1/M5 队列、生命周期、stats 配置 | 并发/满队列/close 单测和目标基准/过载 | M5 主机/ASan全量12/12、ARM构建及板端基准/过载通过 | 板端基准3694条queue drop 0；容量4慢consumer按策略drop 3561；均为单次功能值 | 是，不得写成吞吐、时延或可靠性指标 |
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
`artifacts/20260829T134148+0800-m2-final-audit/`。因此表中的 M2 窄范围描述可用；
板端时钟未初始化，三个正数 timestamp 不能表述为 CAN
时延、UTC 正确性或性能。

M3 preflight 为 `artifacts/20260829T141730+0800-m3-preflight/`。它只证明当时的 M2
前置门禁和环境状态，仍保持历史含义。此后仓库已加入 STM32 `.ioc`、Keil 工程和业务
源码；项目所有者确认 Keil Build、接线/终端和物理 `candump` 正常，并选择不补建新的
M3-A～M3-D artifact。因此可使用表中受限的 STM32/物理 CAN 描述，但不得声称仓库有
未实际归档的日志。M3-E 后续两次1110帧真实接收均无接收错误；项目所有者取消10分钟
门禁并关闭 M3，但仍不得写10分钟 `gatewayd`、DBC、可靠性或性能结论。

Windows M3-A 前置审计
`artifacts/20260829T144926+0800-m3a-preflight-windows/` 仍只代表当时的 BLOCKED 状态，
不作为后续 PASS artifact，也没有被改写。当前进度依据是已同步源码和项目所有者的
简化验收陈述及两次短时 `gatewayd` 输出；M3 已由项目所有者验收关闭。连续10分钟仍
为 NOT RUN/已豁免。

M4 主机证据为 `artifacts/20260830T205736+0800-m4-host-final/`，默认 warning-clean
构建和全量 CTest 11/11 PASS；ASan+UBSan 证据为
`artifacts/20260830T205834+0800-m4-asan-ubsan/`，同样11/11 PASS，LeakSanitizer 为
`NOT RUN`。ARM 交叉构建证据为 `artifacts/20260830T205937+0800-m4-arm-cross/`，只证明
静态解码源码能生成 ARMv7 hard-float binary，没有部署或板端运行。当前语义主机证据
`artifacts/20260831T112833+0800-m4-host-semantic-final/` 覆盖42条向量和6660帧模型；
实物证据 `artifacts/20260831T111733+0800-m4-stm32-physical-final/` 覆盖60秒
6000/600/60帧且逐帧匹配。这些是功能测试规模，不是吞吐、时延或可靠性指标。M4 结束
时必须把“主机静态解码验证”与“M2 binary 的板端 SocketCAN 验证”分开表述；后续 M5
已用新的、SHA256 固定的 ARM binary 补齐实时集成证据，但仍只到 mock sink。

项目所有者确认 Keil Build 0 error/0 warning且已Download，但完整原始 Keil 输出未归档；
实物捕获也没有固件镜像哈希。因此可写“语义化 STM32 输入与自定义 DBC 离线解码预期
逐帧一致”，不能写“源码与烧录镜像已哈希绑定”。M4 当时不能写“新增解码器已在
i.MX6ULL 实时链路运行”；该缺口后来由 M5 板端证据补齐。Windows sanitizer 对新增
语义测试的历史 `NOT RUN` 保持不变；当前
Ubuntu clone 已在后述 M5 ASan+UBSan run 中补跑全量测试，但 LeakSanitizer 仍未运行。

M5 前置审计 `artifacts/20260831T122305+0800-m5-preflight/` 重新确认 M4 门禁。Ubuntu
主机最终证据 `artifacts/20260831T123149+0800-m5-host-final/` warning-clean 且全量
CTest 12/12 PASS；定时合成100/10/1 Hz共111帧的主机用例 queue drop 0，容量4、零等待
和2 ms慢消费者的故意过载观测到195次 drop，计数不变量通过，真实 `SIGTERM` 自管道
退出也通过。ASan+UBSan 证据 `artifacts/20260831T123218+0800-m5-asan-ubsan/` 为
12/12 PASS，LeakSanitizer 仍为 `NOT RUN`。ARMv7 交叉构建
`artifacts/20260831T123256+0800-m5-arm-cross/` warning-clean PASS，binary SHA256 为
`567079d01f4fb1e682a959cd01bac3709e4062f42c1f18903596dc47181d0a01`。

项目所有者随后在真实 i.MX6ULL 上运行该 binary，原始日志和验证报告归档于
`artifacts/20260831T132341+0800-m5-board-owner-final/`。基准 capacity 1024、push
timeout 50 ms、sink delay 0：3694条物理输入全部 decode、入队、pop 和消费，queue
drop 为0；过载 capacity 4、push timeout 0、sink delay 20 ms：2970条消费、3561条按
策略 drop、退出时1次 push closed，high-watermark 4/4。两次均记录 signal 15，随后
输出 post-join summary。

项目所有者确认 STM32 在进程启动后才中途开启，故305/75次 receive timeout 是此前
无输入的空闲 poll；不能用进程完整64/69秒计算或声称持续111帧/s。证据未包含
`can_before/after`、正确 UTC 或 shell `wait` 精确退出码，故只支持表中受限的板端实时
mock-sink 集成和队列策略描述，不支持 CAN 错误增量、吞吐、时延、持续运行或可靠性
结论。M5 已通过；MQTT 仍未实现。
