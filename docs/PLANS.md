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
- M6 已于2026-09-01通过。Ubuntu x86_64主机warning-clean和ASan+UBSan全量13/13、
  libmosquitto 2.0.11 loopback 1000-batch基线均通过；随后真实i.MX6ULL动态加载私有
  libmosquitto 2.0.11和SHA256固定的ARMv7 `gatewayd`，以物理CAN输入连接局域网Windows
  Mosquitto Broker。正式subscriber保存1000批、115335条连续记录，batch_seq为1～1000、
  gateway seq为1～115335，missing/duplicate/reordered均为0；gateway和Broker原始日志
  对账为1033次publish与1033次匹配PUBACK，unexpected和MQTT error均为0。Ubuntu独立
  复核确认manifest 131/131、严格validator 8/8及原始Broker/gateway/CAN计数通过。
  正确UTC、时延、吞吐和长期可靠性仍未验证。
- M7 已于2026-09-01通过。除既有Ubuntu普通/ASan+UBSan全量16/16和ARMv7
  warning-clean构建外，真实i.MX6ULL在`/dev/root` ext4专用目录完成Windows Broker
  断线积压、一次核实PID的`kill -9`、同spool重启补传、同进程再次断线重连及tail/
  internal/state损坏恢复。最终subscriber为281个raw batch、71288条raw record、35644条
  raw duplicate、35644条unique seq，missing=0、effective duplicate=0；Broker三段原始
  日志与gateway/subscriber对账均为281次PUBLISH/PUBACK。正确UTC、掉电、性能和长期
  可靠性仍为`NOT RUN`；M8及后续未开始，也未获授权。
- M8已于2026-09-01通过。连续CAN下的poll饥饿和同步reconnect阻塞缺陷修复后，Ubuntu
  warning-clean、全量CTest 17/17、M8专项2/2、ASan+UBSan专项2/2和ARMv7构建/API/RPATH
  复核均通过；真实i.MX6ULL、物理CAN和Windows Broker又完成两次断线、自动重连、一次
  核实PID的SIGKILL、同spool恢复及state损坏安全重放。最终subscriber为323个raw batch、
  51523条raw record、24089条raw duplicate、27434条unique seq，missing=0、effective
  duplicate=0；Broker/gateway/subscriber均为323次PUBLISH/PUBACK。正确UTC、真实掉电、
  性能和长期可靠性仍为`NOT RUN`；M9及后续未开始。
- M9已完成BusyBox部署源码、主机/ASan+UBSan 18/18、M9专项1/1、ARMv7构建及真实板端
  binary/库SHA、备份安装、reload、restart、SIGKILL恢复和BusyBox ash隔离cooldown。
  补充run `artifacts/20260901T230215+0800-m9-manual-postboot-gate/`取得唯一reboot后的新
  boot ID，证明PID 1在无人工start/restart/HUP时自动拉起唯一supervisor。操作者经新增
  授权恢复重启后DOWN的既有CAN基线，再受控start；最终supervisor/child为1/1，child PID
  9951超过60秒不变且身份/SHA/库映射正确。因此M9 BusyBox进程监督门禁为`MET`。
  CAN持久配置、Broker交付、正确UTC和完整无人值守产品ready仍未验证。
- M10已实现BusyBox ash `/proc`每秒采集、Ubuntu CPU/RSS汇总和四场景严格门禁校验。
  warning-clean主机构建、沙箱外全量CTest20/20、最终M10专项2/2、ASan+UBSan全量20/20、
  sanitizer M10专项2/2和ARMv7 warning-clean构建均通过。Windows完成三档profile和Keil
  准备后，Ubuntu复核修正扩展帧文本ID边界，最终分析器8/8、全量CTest21/21，并冻结
  旧`RelWithDebInfo` ARM binary SHA256
  `d234f2c5f0cc732fd56bc43cc2b8f59491944111b430409ca0ab5b6bb07e4fbf`。随后M10 corrective
  prep在独立功能分支实现显式分段spool v2安全回收和有界group commit；当前Debug与
  ASan+UBSan全量均21/21，M10标签5/5，新ARMv7 `RelWithDebInfo` SHA256为
  `07c185e6e7e862195982f37f41501407ca17fd25442ab7b00c224466a8f7be5e`。两个binary均未执行；
  旧SHA已经过期，禁止后续沿用。
  500/1000帧/s各30分钟、20轮每轮5分钟断网、板端`/proc`和24小时基准均为`NOT RUN`。
  M10总门禁为`NOT MET`，本轮停止在M10。

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
| M6 | libmosquitto MQTT QoS 1 基线 | 实际库已验证；至少 1000 batch，seq 和 PUBACK 统计通过 | 2026-09-01 已通过；主机、ARMv7、真实板端物理CAN到局域网Broker及1000-batch validator PASS |
| M7 | 持久化 spool、恢复、重连补传 | 断线/恢复和 `kill -9` 证明顺序恢复及去重后完整 | 2026-09-01 已通过；真实板端/Windows断线、SIGKILL、损坏恢复及raw duplicate validator PASS |
| M8 | 条件式 epoll network reactor | 外部 loop API 可用且保持 M7 行为，否则记录删除 epoll 的决定 | 2026-09-01 已通过；Ubuntu/ARM构建与真实i.MX6ULL/Windows Broker重连、SIGKILL/state恢复、reactor计数及validator PASS |
| M9 | BusyBox 部署与进程恢复 | 开机启动和异常拉起通过，不使用 systemd | 2026-09-01 已通过；主机/ARM、真实安装、init开机拉起supervisor、最终1/1、restart、SIGKILL恢复和ash cooldown PASS |
| M10 | 自动化中断、性能和 24 小时证据 | 压力、断网、`/proc` 指标、稳定性和简历追溯报告齐全 | 工具/离线回归已完成；真实场景均NOT RUN，总门禁NOT MET |

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
11. ARM依赖审计 `artifacts/20260831T135759+0800-m6-arm-dependency-audit/` 确认当时
    Buildroot SDK sysroot没有 `mosquitto.h`、目标库或`.pc`文件，CMake配置退出1；该run
    的ARM交叉构建、部署和板端执行均为`NOT RUN`，没有生成M6 ARM binary。该历史失败
    保持不变，后续以私有构建和板端run补齐，不能倒写为本run通过。
12. 截至上述主机收尾时，局域网跨主机、真实i.MX6ULL物理CAN → MQTT、正确板端UTC及
    目标库精确版本均为`NOT RUN`；host loopback本身不能替代局域网门禁。
13. 收尾审计 `artifacts/20260831T140625+0800-m6-close-audit/` 重跑M6专项单测、
    重放1000条subscriber JSON validator，复核归档源码/二进制哈希、JSON、
    文档格式与M7/M8范围，全部PASS。获批准的只读端口检查确认18884
    无listener；收尾过程没有重新启动Broker。
14. 经项目所有者批准的真实LAN run
    `artifacts/20260831T220718p0800-m6-lan-1000/` 使用SHA256固定的ARMv7 binary及私有
    libmosquitto 2.0.11，从真实i.MX6ULL物理CAN发布到专用有线LAN内的Windows
    Mosquitto 2.1.2。正式subscriber退出0并保存1000批、115335条记录；batch_seq
    1～1000和gateway seq 1～115335均连续，missing/duplicate/reordered均为0。
    gateway最终确认1033批/119082条记录；publish attempt/accepted/matched PUBACK均为
    1033，unexpected、MQTT error和queue drop均为0；Broker原始日志重新计数完全一致。
15. 2026-09-01 Ubuntu独立复核以CRLF兼容的只读方式验证上述LAN run的
    `manifest.sha256`所列131个文件全部匹配，重放当前严格整数validator并通过8/8回归，
    重新解析Broker/gateway/CAN原始证据。CAN前后均为ERROR-ACTIVE、500 kbit/s，RX
    error/drop/overrun及CAN错误类计数增量均为0。`can_accepted=119083`比
    `queue_success=119082`多1帧，原始证据不能独立解释该停止边界差值，因此不外推为
    每个CAN输入均已发布。

M6要求的实际库、ARMv7目标运行、真实物理CAN到局域网Broker、至少1000个QoS 1 batch、
连续unique seq及匹配PUBACK均已有真实证据，退出门禁为 **2026-09-01 MET**。该判定只
证明M6 QoS 1基线功能链路；板端wall clock仍为1970，正确UTC、端到端时延、吞吐、长期
可靠性、Broker精确退出码和停止边界1帧原因均未验证。在M6关闭当时，spool、断线重连、
补传、去重、`kill -9`及epoll仍为`NOT RUN`，属于M7/M8；后续M7结果见下一节。

## M7 完成记录

1. 开始前以`artifacts/20260901T093205+0800-m7-host-pre-broker/`复核M6为`MET`，并记录
   项目所有者在M6关闭后单独批准只执行M7。起始HEAD为
   `f185222bc685e54355b635388bf0094f7ec41b6e`，当时与`origin/master`一致。
2. 新增append-only spool：每条80字节、显式little-endian、magic/version/length/CRC32；
   append逐条`fdatasync`。`spool.state`使用CRC、ACK offset/seq和next batch，按临时文件
   sync、rename、父目录fsync原子更新。data文件使用单写者`flock`和64-bit file offset。
3. 启动扫描会截断部分尾部或最后一条损坏记录；内部损坏拒绝启动。state缺失或损坏时
   回退到offset 0，允许原始重复但不虚报交付。只有匹配MID的PUBACK到达后才推进state。
4. pipeline的下一个gateway seq从data最后有效记录恢复；断线后记录继续持久化，按新增
   `mqtt_reconnect_interval_ms`重试。每次重连重建clean-session客户端并保持最多一个
   in-flight batch；没有使用M8 external-loop/epoll API。
5. stats区分spool append/replay/ACK、tail/state recovery、corruption/error和MQTT错误。
   M7 validator允许内容一致的QoS 1原始重复，但要求按`device_id + seq`去重后连续有序、
   missing=0、effective duplicate=0。
6. 最终离线证据`artifacts/20260901T093654+0800-m7-offline-final/`记录Ubuntu Debug
   warning-clean、沙箱外全量CTest16/16、ASan+UBSan全量16/16和M7标签3/3 PASS；
   LeakSanitizer为`NOT RUN`。恢复validator回归5/5 PASS。
7. 同一源码用Buildroot GCC 7.5.0完成ARMv7 warning-clean交叉构建；binary SHA256为
   `9c4efe5c12f9797e8eda03ada8c6b2162ff0225aa7680c7ac199afdc45382b25`，hard-float、
   依赖目标`libmosquitto.so.1`、无RPATH/RUNPATH。本次板端重新计算部署文件而非沿用
   历史推测值：gateway SHA256与上述值一致；实际libmosquitto 2.0.11 SHA256为
   `b32c8ac4defb2b2920fba2e42f263869508c42e3c1719440db37ffc8d8c2f636`。
8. 跨主机门禁证据为`artifacts/20260901T105414+0800-m7-lan-recovery-gate2/`。spool位于
   `/dev/root` ext4上的专用`/var/lib`目录，不使用`/tmp` tmpfs。Broker断线后cursor停在
   seq4864，spool增长到34309条，即29445条pending；data/state在一次`kill -9`前后hash
   不变。同binary、配置和spool重启后依次补传seq4865～34309，未复用seq。
9. 同一重启进程又经历一次实际Broker停止/恢复并记录`reconnects=1`；后续物理CAN窗口
   新增1335条，正常退出summary为publish/PUBACK 122/122、unexpected 0、queue drop 0、
   spool append/replay/ACK/pending 1335/30780/30780/0。state损坏后又安全重放35644条，
   summary为140/140匹配PUBACK、state recovery 1、pending/corruption/error均0。
10. 尾部副本在offset2851520追加`de ad be ef`后只截断4字节并恢复原hash；内部副本在
    offset80000破坏magic后gateway exit1且在MQTT connect前拒绝；state offset0损坏后
    从头重放，允许原始重复且没有跳过数据。
11. 最终合并subscriber严格验证为raw batch/record 281/71288、raw duplicate 35644、
    unique gateway seq35644、范围1～35644、missing0、effective duplicate0。Broker三段
    gateway PUBLISH/PUBACK为19/19、116/116、146/146，总计281/281，与subscriber一致。
    板端归档SHA256为`51bd1b5c4197c2703ebd10478bd9b39b2ac9b38b991046e95ce5e0dd2f30ae4b`，
    本地复核board manifest 115/115。
12. 限制如实保留：SIGKILL phase1没有最终queue summary；后续正常phase的queue drop为0，
    gateway seq验证无缺失。`can0`从ERROR-ACTIVE变为ERROR-WARNING，最终berr tx/rx为
    0/102而内核RX errors/dropped为0/0。目标wall clock仍为1970；正确UTC、掉电、时延、
    吞吐、介质寿命、compaction和长期可靠性为`NOT RUN`。

实际断线/恢复和一次`kill -9`已经证明积压按序恢复，最终`missing=0`且
`effective duplicate=0`，因此M7总门禁为 **MET**。本结论只关闭M7；M8 external-loop/
epoll以及M9/M10均未实现、未测试、未批准。

## M8 完成记录

1. 项目所有者在M7关闭后单独授权只执行M8。开始前证据
   `artifacts/20260901T125544+0800-m8-preflight/`确认HEAD与`origin/master`均为
   `06dce43fc537365e11f2752aba7eea60098cb259`，M7门禁为`MET`，M9及后续未开始。
2. 同一preflight对与M7板端运行库SHA256相同的ARMv7 libmosquitto 2.0.11进行头文件和
   动态符号审计；四个规范要求的external-loop API以及`mosquitto_want_write`均为5/5
   PASS，因此D-004的条件成立，M8选择实现epoll而不是删除该设计。
3. 新增独立`gateway_mqtt_reactor`：`epoll`监控libmosquitto socket，`eventfd`承载本地
   connect/publish/disconnect唤醒，周期`timerfd`驱动`mosquitto_loop_misc`。socket兴趣集
   按`mosquitto_want_write`动态维护，网络读写只调用external-loop API；生产源码不再调用
   `mosquitto_loop()`。
4. M7单in-flight MID/PUBACK、spool append/ACK cursor、断线重建client、重连deadline和
   seq恢复仍由既有状态机负责；reactor只替换网络推进方式。snapshot和summary新增
   epoll wait、wake/timer/socket事件及read/write/misc调用计数，便于真实run核验。
5. 初始开发证据`artifacts/20260901T125839+0800-m8-host-dev/`为warning-clean构建和M8
   专项2/2 PASS；受限沙箱全量16/17只因PF_CAN权限。Windows门禁随后暴露连续CAN使
   poll饥饿、同步connect阻塞两项M8缺陷，失败run分别保存在
   `artifacts/20260901T143500+0800-m8-windows-reactor-gate/`和
   `artifacts/20260901T155449+0800-m8-windows-reactor-fixed-gate/`，不得改写为PASS。
6. 修复提交`6c2ed510f75fe8dc762e2ac3586c7cc5a750645e`把poll推进放入consume路径；提交
   `25681d871a32ed3936962144418058d1af2700b4`改为`mosquitto_connect_async`和deadline
   驱动状态机，并新增静默listener不发CONNACK、未显式poll、consume低于250 ms且持续
   reconnect的回归。最终Ubuntu证据
   `artifacts/20260901T160813+0800-m8-silent-listener-reconnect-fix-ubuntu/`为warning 0、
   全量CTest 17/17、M8 2/2、ASan/UBSan 2/2和ARMv7/API/RPATH PASS；LeakSanitizer为
   `NOT RUN`。
7. 早期ARM证据`artifacts/20260901T130048+0800-m8-arm-cross/`保留两次前置失败：首次交叉
   查找未定位开发集，第二次binary带构建机RPATH。最终显式匹配目标头文件/库并禁止
   RPATH后warning-clean构建PASS；ELF为ARMv7 EABI5 hard-float，依赖`libmosquitto.so.1`、
   无RPATH/RUNPATH，SHA256为
   `2c3841e6a18ea80a470bf7d2bb8deaed314fdd1a495dc8c2b5c9a4021a8a9a6b`。最终门禁使用的新
   binary为198576 bytes，SHA256
   `2e3976727d57f850223ec3b0b3713c930d96f75375897f7c1fe69dcfc2e1548b`，并新增
   `mosquitto_connect_async`，external-loop动态引用6/6、无RPATH/RUNPATH。
8. 第三个Windows run
   `artifacts/20260901T163809+0800-m8-windows-reactor-async-gate/`已通过硬件恢复行为，
   但无持久化Broker重启后gateway早于clean-session subscriber连接，4个低序号batch直到
   state重放才首次被捕获，严格validator因first-seen乱序exit 1。该run保持FAIL，JSONL
   未重排、validator未放宽。
9. 最终证据`artifacts/20260901T170636+0800-m8-windows-reactor-final-gate/`使用全新ext4
   spool，并明确证明subscriber先于gateway重连。两次离线窗口均表现为CAN/spool增长而
   ACK/state冻结；一次核实PID/comm/exe/config的SIGKILL返回0且wrapper exit137；同spool
   `main2`和state损坏副本均最终pending0、drop0、unexpected PUBACK0、优雅exit0。
10. `main2`和`state1`的epoll/wake/timer/socket/read/write/misc计数均非零；Broker三段
    31+112+180=323次gateway PUBLISH/PUBACK与subscriber 323行完全一致。严格validator
    对51523条raw record得到24089条raw duplicate、27434条unique seq、missing0、
    effective duplicate0，exit0。因此M8总门禁为 **MET**。
11. 正确UTC、真实掉电、存储掉电语义、性能/时延/吞吐、CPU/RSS、介质寿命、容量阈值、
    compaction和长期可靠性仍为`NOT RUN`。M9及后续未开始，进入M9仍需项目所有者另行
    授权。

## M9 完成记录

1. 用户在M8关闭后单独授权只执行M9。前置run
   `artifacts/20260901T175107+0800-m9-preflight/`确认HEAD与origin一致，M8最终validator
   和201项manifest均PASS，M8门禁为`MET`，M10未开始。
2. M9选择BusyBox `inittab`的respawn动作运行前台supervisor，不使用systemd，也不把
   脚本命名成rcS自动扫描的`S??gatewayd`。supervisor只启动一个`gatewayd --run-mqtt`，
   处理TERM/HUP、PID/lock、stop/start和受控restart。
3. 快速失败达到可配置阈值后进入固定cooldown，稳定运行达到阈值后重置计数；该设计
   避免无间隔respawn风暴，但真实目标BusyBox行为仍须上板验证。
4. 主机证据`artifacts/20260901T175412+0800-m9-host-final/`为warning-clean构建和沙箱外
   全量CTest18/18、M9专项1/1。首次缺少显式libmosquitto路径的配置exit1，以及沙箱内
   PF_CAN/TCP权限导致的16/18均原样保留。
5. `artifacts/20260901T175610+0800-m9-asan-ubsan/`为ASan+UBSan全量18/18；LSan因ptrace
   环境为`NOT RUN`。`artifacts/20260901T175900+0800-m9-busybox-supervisor-final/`保存
   BusyBox ash完整trace：异常退出42后重拉、restart更换PID、stop/start和3次快速失败
   后2秒cooldown均通过。
6. ARM run `artifacts/20260901T175700+0800-m9-arm-cross/`保留相对SDK路径配置失败和首次
   成功构建却含构建机RPATH的审计失败；最终以`CMAKE_SKIP_RPATH=TRUE`重建，warning-clean、
   ARMv7 hard-float、目标解释器/依赖和无RPATH/RUNPATH均PASS。binary SHA256为
   `6e8729417b3dc40c10a413459de5eca9be43ce58dfcc8a3b12e91f5c8d7ef958`。
7. `artifacts/20260901T180003+0800-m9-board-not-run/`明确记录真实目标板门禁为`NOT RUN`：
   当前Ubuntu会话没有目标板端点/凭据/已验证传输路径，未修改`/etc`、init、进程或执行
   reboot。所需条件是目标访问路径、指定操作批准及脱敏原始板端证据。
8. 正式artifact前两次直接BusyBox开发测试没有进入唯一run，明确不作为门禁证据；所有
   结论只引用后续正式run。M10压力、性能、`/proc`和24小时测试均未实现或执行。
9. Windows续跑`artifacts/20260901T182509+0800-m9-windows-board-gate/`从真实目标只读
   证明PID 1是链接`libbusybox.so.1.31.1`的standalone init，且目标库包含respawn和
   `reloading /etc/inittab`语义；目标时钟仍为1970。指定M9 binary未能从Ubuntu侧认证
   转交，Windows/目标也没有预期SHA文件，因此没有重新计算部署输入hash，并按约束在
   staging、`/etc`、HUP/reboot和进程信号前停止。结束审计证明inittab hash未变、M9
   staging不存在、supervisor/gatewayd为0/0且无遗留测试进程；这不是一/一服务PASS。
10. 最终续跑`artifacts/20260901T204152+0800-m9-windows-board-gate-final/`重新计算板端
    `/tmp/m9-staging/gatewayd`为104860 bytes、SHA256
    `6e8729417b3dc40c10a413459de5eca9be43ce58dfcc8a3b12e91f5c8d7ef958`，并确认ARMv7
    hard-float、目标解释器、三项NEEDED和无RPATH/RUNPATH；匹配libmosquitto SHA256为
    `b32c8ac4defb2b2920fba2e42f263869508c42e3c1719440db37ffc8d8c2f636`。
11. 同一run在原inittab hash不变、受管路径不存在和进程0/0前提下创建ext4本轮备份/
    spool目录，安装`/opt/gatewayd`及四个授权`/etc`文件。HUP PID 1后supervisor/真实
    gatewayd binary为1/1；首版仅按comm计数误把shell supervisor也计为gatewayd而FAIL，
    后续exe+SHA+cmdline分类PASS且没有不相关gatewayd。
12. 核验后的子PID 11348被一次SIGKILL后由同一supervisor 11337替换为14335；随后受控
    restart把14335替换为15095，动作exit0、旧PID消失、新PID稳定且始终1/1。目标BusyBox
    1.31.1 ash隔离fake测试在3次快速失败后进入2秒cooldown，1秒窗口内没有第4次启动，
    随后恢复；测试临时进程/目录清理为0且生产PID不变。
13. reboot前boot ID、PID、inittab和持久化marker已保存。唯一一次`sync; /sbin/reboot`
    已发出；首版本地轮询器因预期SSH断线触发PowerShell终止异常，恢复脚本没有再次
    reboot并执行48次只读探测，但均exit255，无法读取新boot ID。开机自启、最终1/1、
    post-boot CAN/Broker和回滚因此`NOT RUN`；目标wall clock此前仍为1970，Broker连接
    只观察到SYN-SENT且未被修改。
14. 手动补充run `artifacts/20260901T230215+0800-m9-manual-postboot-gate/`随后取得新boot ID
    `0abefcf0-9d85-4a4b-b335-f339b33b8db4`。操作者确认首次检查前未人工start/restart/HUP；
    supervisor PID 337、PPID 1且cmdline匹配inittab，因此init自动拉起为PASS。
15. post-boot CAN初始为DOWN/STOPPED，supervisor存在但child为0。为避免真实binary继续
    周期尝试，操作者先受控stop；在新增明确授权后只恢复既有500000 bit/s、loopback off、
    UP基线，前后统计均保存，没有修改Broker、网络配置或STM32。
16. 最终受控start exit0；5秒及超过60秒status均exit0，supervisor/child精确1/1，child
    PID 9951不变，PPID、exe、cmdline、预期binary SHA及固定libmosquitto映射通过，其他
    gatewayd/测试进程为0；CAN最终UP/ERROR-ACTIVE、berr 0/0。

因此基础run与补充run组合覆盖init开机拉起supervisor、最终1/1、restart换PID、异常拉起、
隔离storm cooldown和最终正常状态，M9总门禁为 **MET**。该结论只覆盖BusyBox进程监督；
CAN持久启动配置、Broker交付、正确UTC、完整无人值守产品ready、性能和可靠性仍不成立。
本轮停止在M9，不进入M10。

## M10 执行记录

1. 用户在Windows端完成M9后授权本轮只执行M10。前置run
   `artifacts/20260901T233553+0800-m10-preflight/`确认HEAD与`origin/master`均为
   `645d6842facf8b6a2a9bf00b02b79a7c602657fd`；4组既有未跟踪证据保持不动。M9基础run
   的生成端manifest自检为109项0不匹配，post-boot补充run为29项0不匹配；组合门禁为
   `MET`。Ubuntu直接复核Windows文本hash会受M9未设置`-text`的CRLF/LF规范化影响，该
   限制已记录且未伪装成跨clone逐字节PASS。
2. `tools/metrics/collect_proc_metrics.sh`使用POSIX/BusyBox ash读取固定PID或PID文件，
   每个样本保存epoch、`/proc/uptime`、进程starttime/CPU ticks、系统CPU ticks、VmRSS、
   VmHWM、Linux state和采样状态；可核对exe，进程缺失/身份不符/读取错误均可见，且
   拒绝覆盖已有输出。
3. `report_proc_metrics.py`按相邻同一PID/starttime样本计算总系统CPU容量口径，CPU、
   VmRSS和VmHWM报告平均、nearest-rank P95及最大值。`validate_m10_run.py`只接受
   `imx6ull-physical`：500/1000帧/s场景各至少1800秒、20轮断网每轮至少300秒、111帧/s
   基准至少86400秒。指标必须覆盖完整时长、至少`duration+1`个样本，最大间隔不超过
   2.5秒；三类CAN ID必须分别统计帧数/gap，总数达到速率乘时长并与最终MQTT unique
   record相等；CAN error、queue drop、MQTT missing/effective duplicate和进程退出为0。
4. 主机run `artifacts/20260901T234852+0800-m10-host-final/` warning-clean。首次受限沙箱
   全量CTest为18/20，两个FAIL分别是PF_CAN和TCP socket权限，原日志保留；沙箱外v2为
   20/20 PASS；最终工具源码全量v4再次为20/20。M10专项v4为2/2 PASS，实际使用Ubuntu
   BusyBox 1.30.1 ash，
   并以合成CSV/JSON覆盖正常/拒绝规则。合成86401行只测试24小时合同解析，不是24小时运行。
5. ASan+UBSan run `artifacts/20260901T235040+0800-m10-asan-ubsan/`全量v2和最终v4均为
   20/20，M10专项v4为2/2；LeakSanitizer因ptrace环境为`NOT RUN`。ARM run
   `artifacts/20260901T235152+0800-m10-arm-cross/` warning-clean，binary为ARMv7 EABI5
   hard-float、解释器/依赖正确、无RPATH/RUNPATH，SHA256为
   `7bb1d7299eac43d5a7a9b8f52981652c6ed3e3f3b29567ff74a1abc5f2b3edef`；未部署或板端执行。
6. `artifacts/20260901T235415+0800-m10-board-not-run/`明确记录：本会话没有真实i.MX6ULL、
   STM32、Windows Broker或subscriber端点/凭据/已验证传输路径，也没有本轮具体外部状态
   操作条件。因此500/1000帧/s各30分钟、20轮5分钟Broker断网、板端`/proc`采样和24小时
   基准全部`NOT RUN`；没有修改CAN、网络、`/etc`、init、进程、Broker、固件或依赖。
7. Windows从远端`f67b3652e382dc41d97750b74df71b5d6d1d88cb`继续准备。项目所有者确认采用
   推荐整数profile：111档100/10/1帧/s、500档450/45/5帧/s、1000档900/90/10帧/s；
   压力档采用100-slot、2 ms/1 ms确定性调度，状态模型仍按10 ms推进。正式gatewayd
   选择由Ubuntu新建`RelWithDebInfo`，不沿用现有Debug SHA作为性能输入。
8. 新增M10 candump分析器及7项无硬件回归，覆盖三档速率/每ID计数、counter、DLC、XOR、
   压力slot序列、意外/error frame、CAN前后状态和旧M4 6660帧合同；Windows直接执行7/7
   PASS，并把测试注册到CTest；当时等待Ubuntu执行全量复核，后续结果见第11～14项。
9. `artifacts/20260902T094824+0800-m10-windows-profile-prep2/`保存三个Keil target的
   ARMCC 5.06u6全量rebuild，均0 error/0 warning；六个`.axf/.hex`完成大小/SHA256复核，
   构建产物被忽略且不提交。首次脚本run因PowerShell stderr捕获错误停止，保留在
   `artifacts/20260902T094551+0800-m10-windows-profile-prep/`，其中Keil build为`NOT RUN`。
10. 本次未烧录、未修改目标/CAN/Broker/网络/进程，未执行短硬件预演或长时间测试。Windows
    准备提交`06eaf8efafe126f74330fc60dbd291b1dffe1cfe`和交接提交
    `b25cab851c2daf8e7b19d6eb3338747d400d06c8`已push并停止。
11. Ubuntu以fast-forward拉取到`b25cab851c2daf8e7b19d6eb3338747d400d06c8`，四组既有
    未跟踪目录未动。四个Windows manifest直接`sha256sum -c`因CRLF使文件名含`\r`均
    exit 1；未改写原件，改用`tr -d '\r'`只读输入后68/68 PASS。原失败与兼容复核保存在
    `artifacts/20260902T101743+0800-m10-ubuntu-review/`。
12. 代码复核发现can-utils以8位文本ID表示EFF帧，原分析器可能把`00000100`错误接受为
    标准`0x100`。现保留文本宽度并恢复EFF语义，新增拒绝回归；最终分析器8/8 PASS，
    已归档M4真实6660帧抓包重放仍PASS。
13. Ubuntu Debug warning-clean构建PASS。受限沙箱CTest为19/21，两个FAIL仍是PF_CAN和
    保留socket权限；沙箱外最终当前源码21/21、M10标签3/3 PASS。额外主机
    `RelWithDebInfo`诊断构建因glibc FORTIFY在既有`write`返回值和时间戳`snprintf`处产生
    warning且项目启用`-Werror`而FAIL，原日志保留，未冒充PASS。
14. 正式ARMv7 `RelWithDebInfo`配置、构建和clean verbose rebuild均PASS，实际参数包含
    `-O2 -g -DNDEBUG -Wall -Wextra -Wpedantic -Werror`。binary为EABI5 hard-float ELF32，
    解释器`/lib/ld-linux-armhf.so.3`，NEEDED为`libpthread.so.0`、`libmosquitto.so.1`、
    `libc.so.6`，无RPATH/RUNPATH；两次SHA一致，为
    `d234f2c5f0cc732fd56bc43cc2b8f59491944111b430409ca0ab5b6bb07e4fbf`。binary未提交、
    私有传输、部署或板端运行。
15. Ubuntu准备停止点已关闭，但这不授权硬件操作。STM32烧录、三档短candump、120秒板端
    指标预演、短端到端对账、两个30分钟压力run、20轮Broker中断和24小时基准仍全部
    `NOT RUN`。Windows必须先拉取包含分析器修复的最终提交，再按清单逐项取得精确授权。
16. 最终复跑发现文档化生成器命令先后使用旧脚本名、遗漏必需`--project`，两次均exit 2；
    失败保存在`artifacts/20260902T103243+0800-m10-ubuntu-final-rerun/`。补齐真实脚本和
    工程路径后canonical target检查PASS；分析器8/8及沙箱外CTest21/21也再次PASS。
17. corrective prep前置检查确认M9仍为`MET`，tracked工作树干净且四个既有未跟踪目录
    未动；fetch/pull均fast-forward，HEAD与`origin/master`严格等于
    `6ee5d475f5451e6cba72f0041613009ed9fc9250`。随后创建独立分支
    `m10-spool-v2-reclaim`，未直接修改或merge `master`。
18. 先冻结`docs/milestones/M10_SPOOL_V2_DESIGN.md`。显式`spool_format=v2`使用全新
    目录、GSP2/GST2 CRC、65536条/5242880字节segment和state tmp+fdatasync+rename+
    parent fsync事务。只有全segment ACK且ACK state先持久化才删除，并再次fsync目录；
    legacy默认、格式和历史证据不改，未知/缺失/乱序格式fail closed。
19. 第一提交实现分段滚动/顺序读取/回收、永久sequence/batch/segment高水位、256 MiB
    默认容量上限和明确capacity错误，并保持逐条`fdatasync`。测试覆盖pending清零后重启
    序号不复用、活跃尾部、内部CRC、缺失/乱序和append/sync/state/rename/delete故障。
20. 第二提交增加`spool_sync_records`/`spool_sync_interval_ms`。默认1/1000保持严格语义；
    M10候选128/1000以先到者为准。Broker离线poll、记录/时间阈值、segment滚动、publish
    前和正常关闭均flush；sync失败向上传播。reservation允许崩溃后出现缺口但禁止seq复用。
21. 最终Debug证据`artifacts/20260902T121149+0800-m10-spool-v2-host-final/`为warning-clean、
    全量CTest21/21，标签M7 4/4、M8 2/2、M9 1/1、M10 5/5。ASan+UBSan证据
    `artifacts/20260902T121150+0800-m10-spool-v2-asan-ubsan/`全量21/21且无诊断；
    LeakSanitizer仍为`NOT RUN`。
22. ARM证据`artifacts/20260902T121151+0800-m10-spool-v2-arm-relwithdebinfo/`完成
    `RelWithDebInfo` clean verbose warning-clean rebuild、ELF/解释器/NEEDED/无RPATH检查，
    前后SHA均为`07c185e6e7e862195982f37f41501407ca17fd25442ab7b00c224466a8f7be5e`。
    binary只在忽略的`build/`，未提交、传输、部署或执行。
23. 本轮没有连接、修改或清理板端现有M9 spool，也没有执行STM32烧录、三档短candump、
    120秒板端预演、Broker控制或任何长测。旧M10 binary保留在既有板端非系统暂存位置但
    已过期。所有真实硬件和长时间项目继续`NOT RUN`。

M10离线工具与可执行回归已完成，但退出门禁要求的真实压力、断网、指标和24小时报告不齐，
故M10总门禁为 **NOT MET**。不得产生性能、稳定性、CPU/RSS或长期可靠性简历结论。本轮
立即停止在M10，不进入任何后续阶段。

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
