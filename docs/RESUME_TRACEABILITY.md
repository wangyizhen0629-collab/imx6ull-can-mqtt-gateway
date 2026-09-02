# 简历事实可追溯性

当前仍不能写“完整 CAN--MQTT 网关”、可靠性或性能结论。M1 已证明
x86_64 主机上的配置和并发基础设施单元行为；M2 又完成 ARMv7 warning-clean 交叉构建，
并在真实 i.MX6ULL controller loopback 上证明当前 `gatewayd` 的 SocketCAN 精确过滤、
DLC 拒绝和内核时间戳提取；M4 又完成自定义 DBC、静态 C 解码器、语义化 STM32
模拟信号和实物 CAN 输入一致性闭环。M5 又在真实 i.MX6ULL 上完成物理 CAN → 实时
DBC 解码 → 有界队列 → mock sink 的基准、故意过载和 signal 15 退出验证。M6随后在
真实i.MX6ULL上用物理CAN输入连接Windows Broker，完成1000批连续subscriber数据及
Broker/gateway PUBACK对账，门禁已为`MET`。M7随后在真实i.MX6ULL ext4 spool和Windows
Broker上完成断线、一次`kill -9`、同spool重启补传及损坏恢复，门禁也已为`MET`。这些
仍是单次受控功能验证，不支持“完整网关”、掉电、性能或可靠性结论。M8随后以最终
ARMv7 binary在真实i.MX6ULL、物理CAN、ext4 spool和Windows Broker上完成external-loop
断线重连、SIGKILL/state恢复及严格validator，门禁为`MET`。M9的真实BusyBox进程监督
门禁也为`MET`。M10只完成采集/报告/门禁工具和离线回归，真实压力、断网、CPU/RSS与
24小时场景均为`NOT RUN`，所以仍只允许使用下表明确标为“是”的窄范围表述。

| 候选描述 | 必需源码/配置 | 必需测试 | 必需证据 | 真实值 | 可写简历 |
| --- | --- | --- | --- | --- | --- |
| 为实验用自定义 DBC 实现无动态分配的静态 C 解码器 | M4 `vehicle.dbc`、黄金向量、`vehicle_decoder` | DBC/向量交叉检查、边界/错误、语义化 STM32 规律、ARM 构建 | 历史主机/ASan/ARM与当前语义主机/实物CAN证据 | 3类消息；42条向量；6660帧主机模型和实物捕获均匹配 | 是，必须注明自定义协议和离线静态解码 |
| SocketCAN 接收/过滤/内核时间戳与自定义 DBC 解码已在 i.MX6ULL 实时 mock-sink 链路集成 | M2/M4/M5 源码和协议文件 | loopback、过滤、时间戳、黄金向量及板端实时解码 | M2/M4 已通过；M5 主机/ARM及板端物理 CAN 基准/过载通过 | 基准3694条 decode/消费、queue drop 0；仅 mock sink，不含 MQTT、性能或可靠性 | 是，必须注明自定义协议、mock sink 和功能验证限定 |
| 在 i.MX6ULL 上实现 CAN_RAW 精确 ID 过滤、DLC 校验和 SO_TIMESTAMPNS 提取，并以 controller loopback 验证 | M2 `can_receiver`、有限 CLI、toolchain file | 主机错误注入、ARM 构建、真实板端目标/非目标/DLC/timestamp | M2 最终主机、ARM、板端和审计 run | 目标 3/3；非目标 0；DLC reject 1；timestamp 3/3；仅 controller loopback | 是，必须保留 loopback 限定且不得写性能 |
| STM32 确定性模拟 ECU 提供真实物理 CAN 输入 | M3-A/B `.ioc`、Keil 工程和业务源码 | Keil Build、接线/终端检查、`candump`、周期/计数器 | 项目所有者简化验收；原始日志未归档 | PA11/PA12、500 kbit/s；三类 ID/周期/DLC/counter/XOR 正常 | 是，但必须注明为项目实测且不写可靠性/性能数字 |
| pthread 有界生产者--消费者队列及明确过载策略 | M1/M5 队列、生命周期、stats 配置 | 并发/满队列/close 单测和目标基准/过载 | M5 主机/ASan全量12/12、ARM构建及板端基准/过载通过 | 板端基准3694条queue drop 0；容量4慢consumer按策略drop 3561；均为单次功能值 | 是，不得写成吞吐、时延或可靠性指标 |
| Ubuntu x86_64 loopback 上的 libmosquitto QoS 1 单 in-flight batch | M6 `mqtt_sink`、配置、validator | 实际Broker、1000 batch、匹配PUBACK和subscriber seq | M6 host/ASan及loopback集成run | libmosquitto 2.0.11；1000/1000匹配PUBACK；seq 1～1000无缺失/重复 | 是，必须注明host loopback，不能写板端/完整链路 |
| i.MX6ULL物理CAN到Windows LAN Broker的QoS 1基线 | M6 ARM构建、私有目标库、配置和validator | 真实板端、物理CAN、1000 batch、PUBACK及原始证据复核 | M6 LAN run及2026-09-01独立gate review | subscriber 1000批/115335条连续记录；gateway/Broker 1033次publish/PUBACK一致 | 是，必须注明单次功能门禁，不写性能、时延或可靠性 |
| 持久化spool、断线重连补传与进程崩溃恢复 | M7 spool/MQTT/seq恢复源码、格式和配置 | ARM/板端、局域网断线、尾部/internal/cursor损坏、一次`kill -9` | M7离线run及LAN gate2 run | 主机/ASan全量16/16、ARM构建PASS；raw 71288、unique 35644、missing 0、effective duplicate 0 | 是，必须注明真实i.MX6ULL上的单次受控功能门禁；不得写掉电、性能或长期可靠性 |
| epoll 统一 eventfd/timerfd/MQTT socket | M8 reactor、目标API兼容性和计数快照 | 与 M7 等价的 reactor/重连/退出测试 | M8 Ubuntu/ARM证据及最终Windows/i.MX6ULL gate | API兼容、全量17/17、M8/ASan/UBSan 2/2；真实323 batch、unique seq 1～27434、missing/effective duplicate 0，reactor必需计数非零 | 是，必须注明真实i.MX6ULL上的单次受控功能门禁；不得写性能、时延或长期可靠性 |
| BusyBox 开机启动和异常退出恢复 | M9 inittab/foreground supervisor/env及风暴冷却 | 主机状态机、ARM构建、真实启动和受控crash/restart | M9主机/ASan/ARM、目标安装/reload/restart/SIGKILL/ash cooldown及手动post-boot补充run | 新boot ID；PID 1自动拉起supervisor；最终1/1；SIGKILL/restart换PID；3次快速失败后cooldown | 是，但只写真实i.MX6ULL单次BusyBox进程监督门禁；注明CAN基线手工恢复，不写完整无人值守产品ready |
| 压力、重复断网、CPU/RSS 和 24 小时稳定性 | M10 BusyBox采集器、离线报告器、场景validator、三档STM32 profile和candump分析器 | 经批准的500/1000帧/s、20轮断网、每秒/proc及24小时流程 | M10离线run、Windows profile准备run与未来真实场景run | 工具回归及三档Keil rebuild PASS；真实场景全部NOT RUN，无性能/稳定性数值 | 否 |

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
结论。M5已通过；在M5关闭当时MQTT仍未实现，后续M6主机结果如下。

M6前置审计 `artifacts/20260831T134104+0800-m6-preflight/` 重新确认M5门禁。Ubuntu
主机最终证据 `artifacts/20260831T135537+0800-m6-host-final/` 使用实际libmosquitto
2.0.11 warning-clean构建，沙箱外全量CTest 13/13 PASS；ASan+UBSan证据
`artifacts/20260831T135603+0800-m6-asan-ubsan/` 同样13/13 PASS，LeakSanitizer仍为
`NOT RUN`。

经项目所有者明确批准，`artifacts/20260831T135630+0800-m6-mqtt-final/` 在同一Ubuntu
主机上启动仅监听loopback且禁用持久化的临时Mosquitto 2.0.11。1000个单记录测试batch
均被libmosquitto接受并收到匹配MID的PUBACK；unexpected PUBACK为0。subscriber保存
1000条原始JSON，batch_seq和gateway seq均严格为1～1000，missing/duplicate/reordered
全部为0。另一个100 ms测试interval用例证明低流量idle tick可触发到期batch；其
105.236 ms观测值不是性能指标。

上述结果只支持表中的“Ubuntu x86_64 loopback libmosquitto QoS 1功能验证”。ARM依赖
审计 `artifacts/20260831T135759+0800-m6-arm-dependency-audit/` 证明当前Buildroot SDK
缺少目标libmosquitto头文件和链接输入，因此ARM构建、部署、真实i.MX6ULL物理CAN →
MQTT和局域网跨主机测试均为 `NOT RUN`。M6总门禁为 `NOT MET`，不能写成完整网关、
板端QoS、吞吐、时延或可靠性；M7未开始。

最终收尾审计 `artifacts/20260831T140625+0800-m6-close-audit/` 重放上述1000条原始
subscriber JSON，复核M6源码和x86_64二进制hash，并确认没有M7重连/spool或M8
epoll源码。收尾时临时Broker端口18884无listener。这些证据不改变目标侧和
LAN为 `NOT RUN`、M6总门禁为 `NOT MET` 的状态。

上述两段保留M6主机阶段结束时的历史判断。之后的真实LAN证据
`artifacts/20260831T220718p0800-m6-lan-1000/`和独立复核
`artifacts/20260901T090837+0800-m6-lan-gate-review/`已补齐ARMv7、真实i.MX6ULL物理
CAN到Windows Broker门禁：正式subscriber 1000批/115335条连续记录；gateway/Broker
1033次publish/PUBACK对账一致；manifest 131/131和strict validator 8/8通过。M6当前为
`MET`，只支持表中受限的单次功能描述，不支持正确UTC、性能、时延或可靠性。

M7前置复核为`artifacts/20260901T093205+0800-m7-host-pre-broker/`；最终离线证据为
`artifacts/20260901T093654+0800-m7-offline-final/`。实现包括固定格式append-only spool、
CRC、原子PUBACK cursor、尾部/state恢复、gateway seq恢复、重连、单写者锁、独立stats
和去重validator。Ubuntu普通及ASan+UBSan全量均16/16，ARMv7 warning-clean交叉构建PASS。

真实跨主机证据`artifacts/20260901T105414+0800-m7-lan-recovery-gate2/`在i.MX6ULL的
`/dev/root` ext4专用目录和Windows Mosquitto 2.1.2上完成断线积压、一次核实PID的
`kill -9`、同binary/config/spool重启、同进程再次重连、tail/internal/state损坏恢复。
最终subscriber为281个raw batch、71288条raw record、35644条raw duplicate、35644条
unique gateway seq，missing0、effective duplicate0；Broker PUBLISH/PUBACK为281/281。
M7总门禁为`MET`。

可写表述必须保留“真实i.MX6ULL上的单次受控功能门禁”限定。phase1因SIGKILL没有最终
queue summary，正常退出phase的queue drop为0；CAN最终为ERROR-WARNING、berr rx102；
正确UTC、真实掉电、性能、介质寿命、容量/compaction和长期可靠性均未验证。该历史段
不代表后续M8状态；M9及后续仍未开始。

M8开始前证据为`artifacts/20260901T125544+0800-m8-preflight/`：M7门禁为`MET`，并以
实际ARMv7 libmosquitto 2.0.11头文件/动态库确认external-loop API 5/5。开发证据
`artifacts/20260901T125839+0800-m8-host-dev/`和ASan+UBSan证据
`artifacts/20260901T130155+0800-m8-asan-ubsan/`的M8专项均2/2 PASS；受限沙箱全量均
16/17，唯一失败是PF_CAN环境限制。ARM证据
`artifacts/20260901T130048+0800-m8-arm-cross/`最终warning-clean构建PASS、无RPATH/
RUNPATH并引用external-loop符号5/5；binary SHA256为
`2c3841e6a18ea80a470bf7d2bb8deaed314fdd1a495dc8c2b5c9a4021a8a9a6b`。

随后Windows门禁先后暴露连续CAN下poll饥饿和同步connect阻塞；两个失败run及修复历史
完整保留。最终Ubuntu证据
`artifacts/20260901T160813+0800-m8-silent-listener-reconnect-fix-ubuntu/`为warning 0、
全量CTest 17/17、M8专项2/2、ASan/UBSan 2/2及ARMv7/API/RPATH PASS，binary SHA256为
`2e3976727d57f850223ec3b0b3713c930d96f75375897f7c1fe69dcfc2e1548b`。

第三个Windows run因clean-session subscriber重连竞态使严格validator first-seen乱序，
保持FAIL。最终run
`artifacts/20260901T170636+0800-m8-windows-reactor-final-gate/`使用全新ext4 spool并证明
subscriber先于gateway自动重连；真实断线、一次核实PID的SIGKILL、同spool恢复、state
损坏重放、reactor计数和优雅退出均通过。Broker/gateway/subscriber对账为323/323；
validator对51523条raw record得到24089条raw duplicate、27434条unique seq、missing0、
effective duplicate0。M8为`MET`，表中受限epoll表述可用。

正确UTC、真实掉电、性能/时延/吞吐、CPU/RSS、介质寿命、容量/compaction和长期可靠性
仍为`NOT RUN`。该段只描述M8；后续M9状态见下节。

M9前置复核`artifacts/20260901T175107+0800-m9-preflight/`确认M8为`MET`。M9新增BusyBox
inittab respawn前台supervisor、受控restart、stop/start、异常重拉和快速失败cooldown。
主机run`artifacts/20260901T175412+0800-m9-host-final/`warning-clean且沙箱外全量18/18，
ASan+UBSan run`artifacts/20260901T175610+0800-m9-asan-ubsan/`也是18/18；M9标签均1/1。
专项trace`artifacts/20260901T175900+0800-m9-busybox-supervisor-final/`实际覆盖异常退出42、
restart PID替换、stop/start和3次快速失败后2秒冷却。

ARM run`artifacts/20260901T175700+0800-m9-arm-cross/`最终warning-clean、ARMv7 hard-float、
解释器/依赖正确且无RPATH/RUNPATH，binary SHA256为
`6e8729417b3dc40c10a413459de5eca9be43ce58dfcc8a3b12e91f5c8d7ef958`。早期板端NOT RUN和
只读Windows续跑保持历史原义；最终run
`artifacts/20260901T204152+0800-m9-windows-board-gate-final/`已在真实i.MX6ULL实算同一
binary/目标库SHA，完成备份与安装。HUP后精确一个supervisor和一个gatewayd；核验后
SIGKILL把子PID 11348替换为14335，受控restart再替换为15095；目标BusyBox 1.31.1 ash
隔离测试证明3次快速失败后cooldown且可恢复。

基础run在唯一reboot后因SSH不可达而保持`NOT MET`，该历史结论不覆盖。补充run
`artifacts/20260901T230215+0800-m9-manual-postboot-gate/`随后取得不同boot ID；操作者确认
首次检查前未人工start/restart/HUP，PID 337由PID 1自动拉起。重启后的CAN初始DOWN，
操作者在新增授权下恢复既有500000 bit/s基线并受控start；child PID 9951在60秒前后
不变，最终status exit0、1/1、无其他gatewayd/测试进程，binary SHA与库映射正确。

因此组合证据关闭M9为`MET`，表中受限的BusyBox进程监督表述可用。必须同时注明CAN基线
由操作者手工恢复；Broker交付、正确UTC、完整冷启动ready、性能和可靠性仍不成立。

M10前置run`artifacts/20260901T233553+0800-m10-preflight/`复核M9为`MET`。当前新增
BusyBox ash `/proc`采集器、CPU/RSS报告器及严格场景validator；主机run
`artifacts/20260901T234852+0800-m10-host-final/`最终M10专项2/2且沙箱外全量20/20，
ASan+UBSan run`artifacts/20260901T235040+0800-m10-asan-ubsan/`同为全量20/20和最终
专项2/2；ARM run`artifacts/20260901T235152+0800-m10-arm-cross/` warning-clean且无
RPATH/RUNPATH。合成CSV只验证工具，不能写成运行时长或性能。

真实场景run`artifacts/20260901T235415+0800-m10-board-not-run/`明确把500/1000帧/s各
30分钟、20轮每轮5分钟Broker断网、板端`/proc`和24小时基准全部标为`NOT RUN`。因此
M10总门禁为`NOT MET`，表中压力、CPU/RSS与稳定性描述仍不可写简历。

Windows准备随后冻结111/500/1000档为100/10/1、450/45/5和900/90/10帧/s，并增加独立
candump分析器。`artifacts/20260902T094824+0800-m10-windows-profile-prep2/`记录分析器
7/7和三个Keil ARMCC 5.06u6全量rebuild 0 error/0 warning，固件只保存hash而未提交。
这只支持“准备工具和固件可构建”表述；Ubuntu全量复核、`RelWithDebInfo` ARM binary、
烧录/短预演和全部真实场景仍为`NOT RUN`，不改变表中“否”。
