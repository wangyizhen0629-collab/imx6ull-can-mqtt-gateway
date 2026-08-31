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
- M4 已于2026-08-31通过。STM32 使用整数定点生成60秒语义车况并严格按冻结 DBC
  编码；42条黄金向量、完整6660帧主机模型和60秒实物 `candump` 逐帧审计均通过。
  `0x100`/`0x101`/`0x102` 实收6000/600/60帧，payload/counter/XOR/spare-bit 差异及
  CAN 错误计数增量均为0。项目所有者确认 Keil Build 0 error/0 warning 并已 Download，
  但完整原始 Keil 控制台输出未归档。M4 关闭时新增解码器在 i.MX6ULL 实时运行仍为
  `NOT RUN`；该历史缺口后来已由 M5 板端集成证据补齐。
- M5 已由项目所有者于2026-08-31明确授权并完成源码实现。Ubuntu warning-clean
  全量回归12/12、ASan+UBSan 12/12和 ARMv7 warning-clean 交叉构建均通过；主机定时
  合成111帧/s用例 queue drop 为0，故意慢消费者和 `SIGTERM` 用例也通过。项目所有者
  随后在真实 i.MX6ULL 运行 SHA256 固定的 ARM binary：基准窗口3694条全部解码、入队
  和消费且 queue drop 0；慢消费者过载6532条解码输入中2970条消费、3561条 drop、
  1条在关闭竞争中拒绝，计数守恒；两次 signal 15 后均输出 post-join summary。
  timeout 由 STM32 在进程启动后才开启解释，不作为队列失败，也不宣称全程连续负载。
  M5 于2026-08-31通过。
- M6 已获项目所有者授权并完成 Ubuntu x86_64 主机实现与 loopback 基线。实际
  Mosquitto/libmosquitto 2.0.11 下1000个 QoS 1测试 batch 均收到匹配 PUBACK，subscriber
  的 batch_seq/gateway seq 均为1～1000且 missing/duplicate 为0；主机 warning-clean
  和 ASan+UBSan 全量13/13通过。由于正式门禁要求局域网验证，而本次只有同机 loopback；
  且 Buildroot SDK 缺少目标 libmosquitto 开发文件，ARM交叉构建和 i.MX6ULL 运行均为
  `NOT RUN`。因此 M6 实现已完成，但总退出门禁为 `NOT MET`；M7 及后续未开始。

## Milestone 总表

| Milestone | 范围 | 退出门禁 | 状态 |
| --- | --- | --- | --- |
| M0 | 环境审计、架构文档、仓库与主机骨架 | 文档齐全；主机 `gatewayd` 可配置、编译、运行；未知项和 `NOT RUN` 已记录 | 2026-08-28 已通过 |
| M1 | 配置、日志、错误、生命周期、stats、记录类型、有界环形缓冲区 | 主机单元测试覆盖正常、满队列、退出唤醒和并发行为 | 2026-08-28 已通过 |
| M2 | i.MX6ULL SocketCAN loopback 接收、过滤、时间戳 | ARM 交叉编译和真实板端日志证明目标/非目标 ID 过滤及时间戳提取 | 2026-08-29 已通过 |
| M3-A | Windows CubeMX 基础工程 | `.ioc` 已保存；Keil 工程生成成功；Keil Build 成功 | 已完成；项目所有者确认 Build 0 error/0 warning，采用 PA11/PA12 默认映射 |
| M3-B | Windows STM32 确定性模拟 ECU | 三类周期报文、Rolling Counter、XOR 和确定性数据实现；Keil Build 成功 | 已完成；M4 后续语义化模型也已通过实物复核 |
| M3-C | 断电物理层检查 | 接线/模块记录完整；CANH--CANL 终端检查合理 | 已完成；项目所有者确认接线和终端已核对，具体欧姆值未归档 |
| M3-D | i.MX6ULL 物理 CAN 与 `candump` | 烧录 STM32、关闭 loopback；`candump` 看到三类 ID、正确周期和 Rolling Counter | 已完成；项目所有者确认物理 `candump` 及10分钟运行现象正常 |
| M3-E | `gatewayd` 接入真实 CAN | 在真实 `can0` 完成有限接收；原10分钟门禁由项目所有者豁免 | 已完成；两次1110帧 smoke 正常，10分钟测试 NOT RUN/已豁免 |
| M4 | 自定义 DBC、静态解码器、黄金向量 | 主机黄金向量通过，有物理意义的真实 STM32 模拟规律与解码结果一致 | 2026-08-31 已通过；42条向量、6660帧主机模型及60秒实物逐帧审计 PASS |
| M5 | 生产者--消费者链路和 mock sink | 基准负载 queue drop 为 0；过载策略和 SIGTERM 退出通过 | 2026-08-31 已通过；主机/ASan/ARM构建及真实 i.MX6ULL 基准、过载、SIGTERM PASS |
| M6 | libmosquitto MQTT QoS 1 基线 | 实际库已验证；至少 1000 batch，seq 和 PUBACK 统计通过 | 主机实现/loopback 1000-batch PASS；局域网、ARM及板端 NOT RUN，门禁 NOT MET |
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

## M4 完成记录

1. `protocol/vehicle.dbc` 冻结三类 DLC-8 标准帧的 Intel 小端位布局、缩放、偏移和单位；
   byte 6 为各消息独立的 `RollingCounter`，byte 7 为 byte 0～6 XOR。该协议明确是实验
   用自定义协议，不是 OEM 或量产车辆协议。
2. `gateway_vehicle_decode_frame()` 是无动态分配的静态解码器；物理量使用明确的整数
   定点单位。`gateway_vehicle_decode_record()` 通过 `memcpy` 写入原有 32 字节
   `decoded_payload`，成功时设置 checksum/decode 状态位，失败时清零解码区并保留其他
   原始记录状态。M4 没有把该调用接入生产者--消费者线程。
3. 首轮20条向量和旧 byte 递增规律仅作为历史证据保留。当前
   `protocol/test_vectors/vehicle_golden.csv` 有42条向量：31条实物代表帧、8条静态
   边界和3条错误路径；C 单测与 Python DBC 检查器读取同一文件。
4. STM32 在 `USER CODE` 区域使用整数定点实现60秒循环：熄火、启动怠速、加速、
   巡航、减速、停车怠速、熄火。编码严格遵循冻结 DBC 的 Intel 小端和 factor/offset，
   spare bits 清零；只有发送成功后才推进对应 counter 和状态。
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
8. 当前语义模型主机证据
   `artifacts/20260831T112833+0800-m4-host-semantic-final/` 使用 MinGW GCC 6.3.0
   以 C11 和 `-Wall -Wextra -Wpedantic -Werror` 编译并运行通过；42条向量、3条 DBC
   消息和完整6660帧模型均 PASS。该 run 不是 Ubuntu CMake/CTest 或 ARM 证据。
9. 实物证据 `artifacts/20260831T111733+0800-m4-stm32-physical-final/` 保存60秒原始
   `candump` 和前后 CAN 统计。逐帧审计得到6000/600/60帧，模型、counter、XOR、spare
   bits 差异均为0；接收6740包、53920字节，错误计数增量为0。多出的80包位于统计快照
   与 candump 窗口之间，不是捕获丢包结论。
10. 项目所有者确认当前固件真实 Keil Build 为0 error、0 warning并已 Download；完整
    原始 Keil Build/Download 控制台输出没有归档，因此只按 owner confirmation 记录。
    原始 run_id 中的1970年源于目标板时钟未初始化，不代表实际采集日期。
11. M4 当时 Windows 环境的 sanitizer 为 `NOT RUN`：MinGW GCC 6.3.0 缺少 `libasan`，
    UBSan 编译触发编译器内部错误。该历史限制保持原义；当前语义测试后来已由 M5 的
    Ubuntu ASan+UBSan 全量12/12补跑，但不能倒写成 M4 Windows run 的结果。

M4 退出条件已经满足，状态为 **2026-08-31 已通过**。该判定只覆盖自定义 DBC、静态
解码器、语义化 STM32 生成器和实物 CAN 输入一致性。真实 i.MX6ULL 上由 `gatewayd`
实时调用新增解码器在 M4 结束时仍为 **NOT RUN**；该历史边界不代表后续 M5 目标板
门禁已经通过。

## M5 当前记录

1. 新增 `gateway_pipeline`，用一个 producer 线程执行有限超时的 CAN receive、M4
   checksum/DBC 解码和有界入队；一个 consumer 线程从 M1 ring buffer 排空记录并调用
   mock sink。接收回调必须在 `receive_timeout_ms` 内返回，实际 `gatewayd` 使用100 ms
   poll 周期检查退出请求。
2. gateway seq 在每个通过 M2 原始帧校验的记录上推进；解码失败和满队列丢弃仍保留
   seq 缺口。满队列继续采用冻结的“有界等待后丢弃新记录”策略，queue drop 对应
   `GATEWAY_STAT_QUEUE_PUSH_TIMEOUTS`，不与 CAN 接收或解码错误混合。
3. mock sink 校验 timestamp/checksum/decode 状态，统计消费数、seq gap、非单调记录和
   无效记录。`gatewayd --run-mock-sink` 接入真实 `can_interface`，可选
   `--mock-sink-delay-ms` 只用于 M5 故意慢消费者测试；没有实现 MQTT 或持久化。
4. 正常 `SIGTERM`/`SIGINT` 仍通过 self-pipe 进入主线程；随后 request stop、close/
   broadcast、producer 有界退出、consumer drain 和 join，销毁前不留下使用 queue 的
   线程。
5. 前置审计 `artifacts/20260831T122305+0800-m5-preflight/` 重新确认 M4 门禁 MET。
   最终主机证据 `artifacts/20260831T123149+0800-m5-host-final/` warning-clean、全量
   CTest 12/12 PASS；M5 定时合成基准为111帧、queue drop 0、sink consumed 111。
6. 同一主机 run 的故意过载使用 queue capacity 4、push timeout 0、mock sink delay
   2 ms；直接运行观测到200条输入中5条入队/消费、195条按策略 drop、high-watermark 4，
   所有 `push_success + timeout = attempts`、`pop = push_success = consumed` 和关闭后空队列
   不变量通过。该次数受线程调度影响，只是本 run 的功能证据，不是吞吐或性能指标。
7. `SIGTERM` 15 经真实 signal handler/self-pipe 传递后 producer/consumer 均退出并排空；
   ASan+UBSan run `artifacts/20260831T123218+0800-m5-asan-ubsan/` 全量12/12 PASS，
   LeakSanitizer 仍因 `ptrace` 限制为 `NOT RUN`。
8. ARM run `artifacts/20260831T123256+0800-m5-arm-cross/` 使用 Buildroot GCC 7.5.0
   warning-clean 构建成功；输出为 Cortex-A7/ARMv7 EABI5 hard-float ELF32，SHA256
   `567079d01f4fb1e682a959cd01bac3709e4062f42c1f18903596dc47181d0a01`。ARM 构建 run
   本身不含部署；后续板端执行见第9项。

9. 板端最终证据 `artifacts/20260831T132341+0800-m5-board-owner-final/` 归档项目所有者
   从 i.MX6ULL 拉回的两个原始日志。板端 binary SHA256 与 ARM构建一致。基准配置
   capacity 1024/push timeout 50 ms/sink delay 0，3694条物理输入全部 decode、queue、
   pop、consume，queue drop和sink gap均为0，high-watermark 2。
10. 板端过载配置 capacity 4/push timeout 0/sink delay 20 ms，满足
    `2970 success + 3561 drop + 1 push_closed = 6532 attempts`、
    `2970 pop = 2970 consumed`、`3561 drop = 3561 sink gap`，high-watermark 4/4。
    两次均记录 signal 15，随后输出 post-join summary。
11. 项目所有者确认 STM32 是在两个 `gatewayd` 进程启动后才中途开启，因此305/75次
    timeout 是无输入阶段的100 ms空闲 poll。结果不描述为完整64/69秒持续111帧/s；
    本次也没有归档 CAN before/after 或 shell `wait` 精确退出码。
12. 最终关闭审计 `artifacts/20260831T133005+0800-m5-close-audit/` 复核当前源码哈希、
    ARM/板端 binary 身份、两个板端原始日志哈希和文档范围；沙箱内 PF_CAN 环境失败
    如实保留，相同全量 CTest 在沙箱外12/12 PASS，M5 专项直接运行 PASS。

M5 退出条件已经满足，状态为 **2026-08-31 已通过**。该判定覆盖真实 i.MX6ULL 上的
物理 CAN → 实时解码 → queue → mock sink 基准零 drop、故意慢消费者过载策略和
signal 15 graceful shutdown；不产生持续运行、CAN error增量、吞吐、时延或可靠性结论。
上述是M5关闭时的历史边界；本轮项目所有者随后已另行授权M6，当前M6结果见下节。

## M6 当前记录

1. 新增 `gateway_mqtt_sink` 并链接实际 libmosquitto；使用 MQTT 3.1.1 QoS 1、clean
   session 和 `mosquitto_max_inflight_messages_set(..., 1)`。代码不手写 MQTT 协议，
   M6 也不调用 M8 external loop API。
2. batch JSON schema 为 `gateway.telemetry.v1`，包含 device ID、batch_seq、record_count、
   first/last seq及每条记录的原始 CAN、timestamp、状态和解码区。batch 内 seq 必须严格
   递增，允许队列丢弃导致的合法 gap。
3. 默认 `batch_interval_ms=1000`，固定安全上限256条。consumer 使用50 ms timed pop
   idle tick，使低流量下没有下一条记录时仍能发送到期 batch；关闭后先 drain queue，
   再 flush 最后一批并等待 PUBACK。
4. 每次 publish 前统计 attempt，libmosquitto 接受后统计 accepted；只有 callback 的 MID
   与当前单个 in-flight MID 完全相同才统计 matched PUBACK并推进 batch/record确认游标。
   unexpected PUBACK 和 MQTT error 独立统计。
5. M6 没有重连、spool、断线补传或去重。连接/PUBACK失败会令 consumer失败并走现有
   stop/close/join路径；未确认的内存 batch 不被伪装成已交付。恢复语义属于 M7。
6. 配置新增 `mqtt_ack_timeout_ms`，默认5000 ms、允许100～60000 ms；password不能在
   username为空时单独配置。日志继续对用户名和密码脱敏。
7. 前置审计 `artifacts/20260831T134104+0800-m6-preflight/` 重新确认 M5 门禁 MET。
   主机最终 run `artifacts/20260831T135537+0800-m6-host-final/` warning-clean，沙箱外
   全量CTest 13/13 PASS；沙箱内唯一失败仍是已知 PF_CAN权限限制。
8. ASan+UBSan run `artifacts/20260831T135603+0800-m6-asan-ubsan/` 全量13/13 PASS；
   LeakSanitizer 因 `ptrace` 限制为 `NOT RUN`。
9. 经项目所有者明确批准，`artifacts/20260831T135630+0800-m6-mqtt-final/` 使用仅监听
   `127.0.0.1:18884`、禁用持久化的临时 Mosquitto 2.0.11。1000个单记录测试 batch 的
   publish attempt/accepted/matched PUBACK均为1000，unexpected=0；subscriber 原始
   1000条 JSON 的 batch_seq和gateway seq均为1～1000，missing/duplicate/reordered均为0。
10. 同一 run 的低流量定时用例配置100 ms测试 interval，只放入1条记录；idle tick在
    105.236 ms后完成 publish/PUBACK。该数值只证明 timer路径，不作为时延或性能指标；
    正式默认仍为1000 ms。
11. ARM依赖审计 `artifacts/20260831T135759+0800-m6-arm-dependency-audit/` 确认当前
    Buildroot SDK sysroot没有 `mosquitto.h`、目标库或`.pc`文件，CMake配置退出1。因此
    ARM交叉构建、部署和板端执行全部为 `NOT RUN`，没有生成 M6 ARM binary。
12. 局域网跨主机、真实 i.MX6ULL 物理 CAN → MQTT、正确板端 UTC及目标库精确版本均
    `NOT RUN`。本次 loopback不能替代 `docs/TEST_PLAN.md` 要求的局域网门禁。
13. 收尾审计 `artifacts/20260831T140625+0800-m6-close-audit/` 重跑M6专项单测、
    重放1000条subscriber JSON validator，复核归档源码/二进制哈希、JSON、
    文档格式与M7/M8范围，全部PASS。获批准的只读端口检查确认18884
    无listener；收尾过程没有重新启动Broker。

M6 源码和 Ubuntu loopback 基线已经完成，但正式退出条件尚缺局域网/目标环境证据，
所以当前状态为 **NOT MET**。不得据此写成 i.MX6ULL CAN--MQTT 完整链路、吞吐、时延、
可靠性或持久化结果。M7 及后续未开始。

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
