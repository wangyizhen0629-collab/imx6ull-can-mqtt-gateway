# 测试计划与证据规则

## 证据契约

每次实际测试使用唯一的 `artifacts/<run_id>/`。证据按不可变数据管理：未经用户明确
批准，不得删除、截断、替换或改变已有 run 的含义。必须记录命令、配置、开始/结束
时间、软件版本、结果和限制。无法执行的测试写为 `NOT RUN`，并给出原因和前置条件。

板端/集成 run 的核心文件包括 `manifest.json`、`git_commit.txt`、`host_info.txt`、
`board_info.txt`、`gateway.conf`、`can_before.txt`、`can_after.txt`、`gateway.log`、
`subscriber.csv`、`proc_metrics.csv`、`summary.json` 和 `test_report.md`。不适用的项目
保留字段并明确写 `NOT RUN`，不能填入估算值。README 和简历只能引用实际存在的证据。

Windows Keil 门禁也属于同一个证据体系。M3-A/M3-B 至少保留 `.ioc`/`.uvprojx` 的
仓库路径或 commit、Keil 版本、Build 输出、构建时间、PASS/FAIL 和操作者；Codex 无法
亲自执行时必须写 `NOT RUN - 需要用户在 Windows Keil 中验证`，等待用户提供真实输出。

## M0 历史验证

| ID | 检查 | 预期结果 |
| --- | --- | --- |
| M0-HOST-01 | `cmake -S . -B build` | 无外部依赖完成主机配置 |
| M0-HOST-02 | `cmake --build build` | 生成 warning-clean 的 `gatewayd` |
| M0-HOST-03 | `ctest --test-dir build --output-on-failure` | 版本、默认配置、示例配置 smoke test 通过 |
| M0-HOST-04 | 直接运行默认和示例配置 | 输出版本/配置来源并正常退出 |

这些检查只证明 M0 x86_64 主机骨架，不证明 ARM 交叉编译、目标运行、CAN、MQTT、
持久化、并发或性能。历史证据不得因本次规范调整而改写。

## M1、M2 测试组

- M1：配置有效/无效/边界、日志脱敏、生命周期、固定记录结构、FIFO、满队列、
  timed wait、close、并发 ring buffer 不变量。
- M2：i.MX6ULL SocketCAN loopback、目标 ID 接收、非目标 ID 过滤、错误长度处理和
  内核接收时间戳。改变 `can0` 状态前必须批准。

M1 已于 2026-08-28 完成：最终 warning-clean 主机 run 为
`artifacts/20260828T234222+0800-m1-host-final/`，ASan+UBSan run 为
`artifacts/20260828T234154+0800-m1-asan-ubsan/`，均为 8/8 PASS。LeakSanitizer 因
当前 `ptrace` 环境限制为 `NOT RUN`；ARMv7 和板端项目也均为 `NOT RUN`。这些结果不能
代替 M2 的交叉编译和真实板端 SocketCAN 证据。

M2 当前主机最终回归为
`artifacts/20260829T131536+0800-m2-host-regression/`（warning-clean、CTest 9/9 PASS）
和 `artifacts/20260829T131705+0800-m2-asan-ubsan-regression/`（ASan+UBSan、9/9 PASS）。
M2 单测实际覆盖未绑定 CAN_RAW socket 的精确 filter/SO_TIMESTAMPNS 选项、主机内核
辅助时间戳、目标/非目标/flag/DLC/短长 datagram/缺 timestamp/timeout。

ARM 交叉构建 `artifacts/20260829T131442+0800-m2-arm-cross-final/` 使用 Buildroot GCC
7.5.0、glibc 2.30 sysroot warning-clean PASS，输出 ARMv7 hard-float `gatewayd`；首次
feature 宏失败证据保留在 `artifacts/20260829T131347+0800-m2-arm-cross/`。该二进制尚未
在当时的 build run 中执行；后续板端 run 使用相同 SHA256 binary 完成验证。

只读板端审计 `artifacts/19700101T123711+0000-m2-board-audit/` 已 PASS，确认 i.MX6ULL、
ARMv7、Linux 4.9.88、Buildroot 2020.02、FlexCAN `can0` 处于 DOWN/STOPPED，且
`candump`/`cansend` 可用。板端时钟未初始化导致 run_id 为 1970；这不是 loopback、ARM
binary 或 timestamp 功能证据。首次脚本缺失失败保存在
`artifacts/19700101T123312+0000-m2-board-audit/`。

板端部署核验 `artifacts/20260829T132938+0800-m2-board-deploy-verify/` 已 PASS：上传到
`/tmp/gatewayd-m2` 的 SHA256 与 ARM artifact 一致，紧邻测试前 `can0` 为
DOWN/STOPPED 且收发/error 计数为 0。该 run 没有启动程序或修改接口，不是 loopback
功能证据。

真实板端 M2 run 必须在批准后分别执行并判定：三条目标 ID 成功接收且 timestamp 存在；
只发送非目标 ID 时预期 timeout 且零帧；发送目标 ID 的错误 DLC 时记录拒绝并预期
timeout。短/长 SocketCAN datagram 由主机注入单测覆盖，因为正常 CAN 控制器/
`cansend` 不会产生这种 socket datagram。精确命令骨架见 `tools/can/README.md`。

上述真实板端门禁已在明确批准后由
`artifacts/20260829T133148+0800-m2-board-loopback/` 执行并 PASS：目标 3/3、三个正数
递增 timestamp、非目标接受 0、DLC reject 1、CAN error 0，恢复为 DOWN/STOPPED 和
loopback off。最终审计 `artifacts/20260829T134148+0800-m2-final-audit/` 对 18 个 tar
成员逐字节复核并 PASS；首次 UID/GID 比较方法失败的审计 run 保留不变。板端时钟为
1970，timestamp 不作为 UTC/时延结论；500000 bit timing 在 DOWN 状态保留。M2 门禁
据此通过，物理 CAN/STM32 和 M3+ 仍为 `NOT RUN`。

## M3-A～M3-E 测试组

| 阶段 | 环境 | 必须证明 | 不能替代的证据 |
| --- | --- | --- | --- |
| M3-A | Windows CubeMX + Keil | Clock、APB1、PA11/PA12 默认映射、500 kbit/s timing；`.ioc`/Keil 工程；Build 成功 | Ubuntu 工具缺失或源码看起来正确都不能代替 Keil Build |
| M3-B | Windows Keil | `0x100` 100 Hz、`0x101` 10 Hz、`0x102` 1 Hz；Rolling Counter、XOR、确定性信号；Build 成功 | 未执行 Keil 时标记 `NOT RUN` |
| M3-C | 全部断电的真实硬件 | STM32 侧 TJA1050 接线/供电/逻辑电平、i.MX6ULL 板载 TJA1042T/3 CAN 接口、CANH/CANL/GND；CANH--CANL 实测接近 60 Ω | 两端已配 120 Ω 不能代替万用表实测 |
| M3-D | Windows ST-Link + i.MX6ULL 物理 CAN | 经批准烧录并保留结果；经批准关闭 loopback；`candump` 看到三类 ID、周期和 Rolling Counter；CAN 状态可解释 | 不得用 Keil Build 代替烧录，也不得用 `gatewayd` 日志跳过 `candump` 基线 |
| M3-E | i.MX6ULL `gatewayd` | 仅在 M3-D 后接入；至少 10 分钟连续接收，按 ID 统计 counter gap 和 CAN error | `candump` 成功不能自动证明 `gatewayd` 成功 |

M3-C 判定：接近 60 Ω 才符合两只 120 Ω 并联预期；接近 120 Ω 可能少一个终端，
接近 40 Ω 可能有第三个终端。后两者或明显异常时不得上电，应先排查并另建测试记录。

2026-08-30，项目所有者决定对 M3-A～M3-D 采用简化的实际现象验收，不再补建 STM32
侧测试 artifact。已确认 Keil Build 0 error/0 warning、PA11/PA12 物理接线、终端检查、
三类 `candump` 报文及周期/DLC/counter/XOR 正确。该例外只用于记录 M3-A～M3-D 的项目
进度。随后 `gatewayd` 在真实 `can0` 完成两次1110帧短测，两次均 accepted=1110 且
全部接收错误为0；干净复测期间 CAN 状态和错误计数无新增异常。项目所有者明确取消
M3-E 原10分钟/按 ID gap 门禁并接受 M3 完成。连续10分钟仍是 NOT RUN，不得据此声称
稳定性、可靠性或性能结果。

## M4～M10 测试组

- M4：自定义 DBC/黄金向量、信号边界、大小端、缩放和真实确定性数据规律。
- M5：基准零 queue drop、故意慢消费者、队列计数不变量和 SIGTERM 唤醒/退出。
- M6：局域网内至少 1000 个 QoS 1 batch，unique seq 和匹配 PUBACK 统计。
- M7～M8：Broker 断线/恢复、尾部损坏、cursor 恢复、`kill -9`、原始重复、去重后
  完整性，以及可选 reactor 的等价行为。
- M9：BusyBox 启动顺序、受控重启、异常退出恢复和重启风暴防护。
- M10：计划的 500/1000 帧/s 压力、20 轮 5 分钟 Broker 断线、`/proc` 指标和
  24 小时基准稳定性。

M4 首轮20条/768帧历史测试、Ubuntu warning-clean/ASan+UBSan 11/11和 ARMv7 交叉构建
证据保持不变。2026-08-31 又完成语义化门禁：当前42条共享向量包含31条实物代表帧、
8条静态边界和3条错误路径；C 测试重建完整60秒/6660帧整数定点场景，DBC 检查器独立
验证3条消息和全部向量。主机证据为
`artifacts/20260831T112833+0800-m4-host-semantic-final/`。

实物证据 `artifacts/20260831T111733+0800-m4-stm32-physical-final/` 保存60秒
`candump` 和 CAN 前后统计：6000/600/60帧逐帧符合场景、DBC、独立 counter、XOR 和
spare-bit 规则，CAN 错误计数增量为0。Keil 0 error/0 warning及Download为项目所有者
确认，完整原始控制台输出未归档。当前 Windows sanitizer 因旧 MinGW 缺少运行库/内部
错误为 `NOT RUN`；新增解码器在 i.MX6ULL 实时运行也仍为 `NOT RUN`。这些限制不改变
M4 静态协议与真实输入一致性门禁已于2026-08-31通过，但不得外推为 M5 集成结果。

M5 当前 Ubuntu 主机测试 `artifacts/20260831T123149+0800-m5-host-final/` 必须同时验证：

- 定时合成100/10/1 Hz三类输入共111帧，decode/queue/mock sink 全链路 queue drop 为0；
- capacity 4、push timeout 0和2 ms慢消费者造成真实过载，且
  `push_success + push_timeout = attempts`、`pop = push_success = consumed`、
  high-watermark 不超过容量、close 后队列为空；
- checksum 错误不入队并保留可观察的 gateway seq gap；
- 真实 `SIGTERM` 经 signal handler/self-pipe 触发，producer/consumer 唤醒、drain、
  join 后退出。

上述主机 warning-clean 全量12/12已通过；ASan+UBSan
`artifacts/20260831T123218+0800-m5-asan-ubsan/` 也是12/12 PASS，LeakSanitizer 为
`NOT RUN`。ARMv7 warning-clean 交叉构建
`artifacts/20260831T123256+0800-m5-arm-cross/` 已通过；binary SHA256 为
`567079d01f4fb1e682a959cd01bac3709e4062f42c1f18903596dc47181d0a01`。

M5 目标板证据 `artifacts/20260831T132341+0800-m5-board-owner-final/` 归档项目所有者
从真实 i.MX6ULL 拉回的完整 `gateway` 原始日志。基准 capacity 1024、push timeout
50 ms、sink delay 0：3694条物理输入全部 decode/queue/pop/consume，queue drop 为0；
过载 capacity 4、push timeout 0、sink delay 20 ms：6532次尝试中2970次成功、3561次
按策略 drop、退出时1次 closed，high-watermark 4/4。两次均记录 signal 15，随后输出
post-join summary。项目所有者确认 STM32 是进程启动后中途开启，故305/75次 timeout
仅是此前无输入的100 ms poll，不是 queue drop。M5 功能门禁据此为 MET。

本 run 没有保存 `can_before/after`、正确 UTC 或 shell `wait` 精确退出码，这些项目保持
**NOT RUN**。因此不得声称 CAN 错误计数无增量、完整64/69秒持续111帧/s、精确退出码、
吞吐、时延、持续运行或可靠性。今后如需这些结论，必须另行授权并新建唯一 artifact，
在测试开始前启动 STM32，同时保存 binary hash、配置、`can_before/after`、完整日志、
summary 和 shell 退出状态；不得倒填本次 M5 证据。

长时间测试、Broker 控制、接口状态、固件烧录、进程控制和部署操作必须在执行前
单独取得明确批准。

## 统一指标定义

- MQTT missing：积压完全补传后，期望范围内缺失的 unique `device_id + seq` 数量。
- CAN 输入丢失：按 CAN ID 分别统计 STM32 模 256 Rolling Counter gap。
- queue drop、spool drop、MQTT missing 必须分开，禁止合并成一个“丢包率”。
- 原始重复：subscriber 至少两次收到同一 `device_id + seq`；有效重复：validator
  去重输出仍存在重复。QoS 1 不能宣称绝不重复。
- 重连/补传时延由 PC 从 Broker 恢复可连接到收到第一条积压数据测量，报告 P50、
  P95 和最大值。
- CPU 使用每秒 `/proc/<pid>/stat` 与 `/proc/stat` 差值；内存使用 `VmRSS` 和
  `VmHWM`，报告平均、P95、最大值和计算方法。

## 基准和稳定性条件

基准条件为 500 kbit/s、标准 DLC-8、111 帧/s、1 秒 MQTT batch、QoS 1、本地 Broker
和启用 durable spool。只有真实 STM32 与物理总线能够稳定提供相应负载后，才能声称
500 帧/s 或 1000 帧/s 的 30 分钟结果。24 小时报告必须包含 CAN 总数/gap/error、
queue drop、batch/seq/reconnect/duplicate、spool 最大积压、`/proc` CPU/RSS 和进程退出。
