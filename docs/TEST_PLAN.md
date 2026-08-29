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

## M3-A～M3-E 测试组

| 阶段 | 环境 | 必须证明 | 不能替代的证据 |
| --- | --- | --- | --- |
| M3-A | Windows CubeMX + Keil | Clock、APB1、PB8/PB9 Remap、500 kbit/s timing；`.ioc`/Keil 工程；Build 成功 | Ubuntu 工具缺失或源码看起来正确都不能代替 Keil Build |
| M3-B | Windows Keil | `0x100` 100 Hz、`0x101` 10 Hz、`0x102` 1 Hz；Rolling Counter、XOR、确定性信号；Build 成功 | 未执行 Keil 时标记 `NOT RUN` |
| M3-C | 全部断电的真实硬件 | STM32 侧 TJA1050 接线/供电/逻辑电平、i.MX6ULL 板载 TJA1042T/3 CAN 接口、CANH/CANL/GND；CANH--CANL 实测接近 60 Ω | 两端已配 120 Ω 不能代替万用表实测 |
| M3-D | Windows ST-Link + i.MX6ULL 物理 CAN | 经批准烧录并保留结果；经批准关闭 loopback；`candump` 看到三类 ID、周期和 Rolling Counter；CAN 状态可解释 | 不得用 Keil Build 代替烧录，也不得用 `gatewayd` 日志跳过 `candump` 基线 |
| M3-E | i.MX6ULL `gatewayd` | 仅在 M3-D 后接入；至少 10 分钟连续接收，按 ID 统计 counter gap 和 CAN error | `candump` 成功不能自动证明 `gatewayd` 成功 |

M3-C 判定：接近 60 Ω 才符合两只 120 Ω 并联预期；接近 120 Ω 可能少一个终端，
接近 40 Ω 可能有第三个终端。后两者或明显异常时不得上电，应先排查并另建测试记录。

## M4～M10 测试组

- M4：自定义 DBC/黄金向量、信号边界、大小端、缩放和真实确定性数据规律。
- M5：基准零 queue drop、故意慢消费者、队列计数不变量和 SIGTERM 唤醒/退出。
- M6：局域网内至少 1000 个 QoS 1 batch，unique seq 和匹配 PUBACK 统计。
- M7～M8：Broker 断线/恢复、尾部损坏、cursor 恢复、`kill -9`、原始重复、去重后
  完整性，以及可选 reactor 的等价行为。
- M9：BusyBox 启动顺序、受控重启、异常退出恢复和重启风暴防护。
- M10：计划的 500/1000 帧/s 压力、20 轮 5 分钟 Broker 断线、`/proc` 指标和
  24 小时基准稳定性。

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
