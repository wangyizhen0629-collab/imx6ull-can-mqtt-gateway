# 架构决策记录

## D-001：当前目录直接作为项目根目录

- 日期：2026-08-28
- 状态：已接受
- 依据：该目录是独立 Git worktree，初始无 commit、remote 或业务文件。
- 影响：直接创建项目结构，不移动或删除无关文件。

## D-002：M0 Linux 主机骨架使用 C11 和 CMake

- 日期：2026-08-28
- 状态：M0 已接受
- 原因：Ubuntu 已有 GCC 11.4.0、CMake 3.22.1 和 GNU Make 4.3；后续可通过
  toolchain file 接入 Buildroot 交叉编译器。
- 影响：M0 主机构建不能证明 ARM 交叉编译；最低 CMake 版本暂定 3.16。

## D-003：M0 不增加第三方运行依赖

- 日期：2026-08-28
- 状态：已接受
- 原因：当时未发现 libmosquitto 开发文件或目标 sysroot，且安装/下载依赖需要批准。
- 影响：M0 程序只检查配置文件可读性；语义解析属于 M1，MQTT 属于 M6。

## D-004：epoll 保持条件式设计

- 日期：2026-08-28
- 状态：推迟到 M8
- 原因：目标 libmosquitto 版本和 external loop API 尚未确认。
- 影响：先建立 M6/M7 基线；如果目标集成无法合理验证，就从实现和简历中删除 epoll。

## D-005：证据先于项目和简历结论

- 日期：2026-08-28
- 状态：已接受
- 原因：QoS 1 允许原始重复，不同链路位置的丢失必须分开统计。
- 影响：源码、测试、artifact 和真实数值完整映射前，简历描述保持不可用。

## D-006：STM32 与 Linux 使用双开发环境、单一 Git 可信源

- 日期：2026-08-28
- 状态：已接受
- 决定：Windows 使用 STM32CubeMX + Keil MDK + ST-Link；Ubuntu 负责 i.MX6ULL
  `gatewayd`、交叉编译和 Linux 测试。两边分别 clone 同一仓库，仓库源码是唯一可信源。
- 影响：STM32 真实编译以 Keil Build 为准；Ubuntu 不负责 STM32 编译，也不因缺少
  `arm-none-eabi-gcc`/OpenOCD 阻塞 Linux 侧阶段。禁止维护多份独立 STM32 源码。

## D-007：STM32 定位为最小确定性模拟 ECU

- 日期：2026-08-28
- 状态：已接受
- 决定：STM32 只实现 CAN 初始化、三类周期帧、Rolling Counter、XOR、确定性信号及
  必要调试/错误统计；不引入 RTOS、USB 协议、GUI、复杂 bootloader、TCP/IP、MQTT
  或文件系统。
- 影响：主要技术复杂度继续集中在 i.MX6ULL Linux 侧。CubeMX 自定义逻辑优先放在
  `USER CODE BEGIN/END` 区域；M3 拆分为 M3-A～M3-E 顺序门禁。

## D-008：物理 CAN 使用两个 TJA1050 模块及其标称终端

- 日期：2026-08-28
- 状态：已被 D-010 更正
- 决定：STM32 和 i.MX6ULL 各连接一个 TJA1050 模块，两个模块均标称自带 120 Ω，
  位于总线两端；禁止再额外并联 120 Ω。
- 影响：此前“是否缺少第二个收发器/终端模块”的问题关闭。M3-C 仍必须全部断电实测
  CANH--CANL，约 60 Ω 才符合预期；约 120 Ω 或 40 Ω 时停止上电并排查。

## D-009：项目文档和人工维护注释默认中文

- 日期：2026-08-28
- 状态：已接受
- 决定：文档使用中文解释并保留必要英文专有名词；代码标识符、文件名、API、topic
  和协议字段名保持英文。人工注释重点解释设计原因、并发约束、生命周期和边界条件。
- 影响：禁止拼音/中文标识符和无价值逐行注释；旧文档按优先级渐进转换，不因翻译
  重写已有正确代码或历史测试证据。

## D-010：更正 i.MX6ULL 板载 CAN 模块组成

- 日期：2026-08-28
- 状态：已接受，取代 D-008 中的 i.MX6ULL 侧 TJA1050 描述
- 决定：STM32 侧继续使用带 120 Ω 的外置 TJA1050 模块；i.MX6ULL 开发板已经配备
  完整 CAN 模块，包含 CAN 控制器、TJA1042T/3 收发器、120 Ω 终端、TVS 保护和
  CANH/CANL 接口，不再外接第二只 TJA1050。
- 影响：两端各有一个 120 Ω，仍禁止额外并联终端；M3-C 保留断电实测约 60 Ω 的门禁，
  但“i.MX6ULL 是否具备收发器/终端/TVS/CAN 接口”不再是未知项。

## D-011：M1 配置采用严格 key=value schema 和分层覆盖

- 日期：2026-08-28
- 状态：M1 已接受
- 决定：配置只接受逐行 `key=value`、空行和行首 `#` 注释；未知 key、同一文件重复
  key、格式错误和越界值均拒绝。优先级固定为“内建默认值 < 配置文件 < 按命令行顺序
  应用的 `--set KEY=VALUE`”。当前 schema 包含 device/CAN/Broker/topic、队列超时与
  容量、batch 周期、spool 路径和日志等级。
- 安全边界：配置日志始终将 `broker_username` 和 `broker_password` 脱敏；解析错误不
  回显 value。真实凭据优先放在权限受控且不提交的配置文件中，因为命令行参数仍可能
  被本机进程列表观察到。
- 影响：后续新增或改变 key、默认值、范围或优先级必须同时更新示例、单元测试和决策
  记录；M1 只解析这些参数，不连接 CAN、Broker 或 spool 文件。

## D-012：M1 并发基础设施使用 mutex/condition variable 和自管道生命周期

- 日期：2026-08-28
- 状态：M1 已接受
- 决定：64-bit stats 全部在 mutex 下读写，不假设 ARMv7 lock-free；ring buffer 使用
  `while` 谓词等待、`CLOCK_MONOTONIC` 有界 producer timeout、close/broadcast，关闭
  后允许消费者先排空已有记录。信号 handler 只向 nonblocking self-pipe `write()`，
  普通线程再设置 stop 状态和广播条件变量。
- 影响：队列 push 超时由调用方视为“丢弃新记录”的边界，M5 才接入真实生产者/消费者
  并验证基准负载。销毁 ring buffer 前必须先 close 并 join 所有使用线程。

## D-013：M1 冻结 64 字节 telemetry_record 布局但不定义 M4 解码语义

- 日期：2026-08-28
- 状态：M1 已接受
- 决定：记录固定为 64 字节，冻结 gateway seq、内核纳秒时间戳、CAN ID、8 字节原始
  数据、状态、DLC、ECU counter 和 32 字节预留解码区的关键偏移；使用定宽整数和
  `_Static_assert` 检查布局。
- 影响：32 字节解码区在 M1 不承载任何已声明的信号语义。M4 必须依据自定义 DBC 和
  黄金向量同步定义，不能把当前预留区描述成已实现解码。

## D-014：M2 使用内核精确过滤、SO_TIMESTAMPNS 和有限验证入口

- 日期：2026-08-29
- 状态：M2 已接受并经板端门禁验证
- 决定：Linux 接收端使用一个 `CAN_RAW` socket，为 `0x100`、`0x101`、`0x102` 安装
  `CAN_SFF_MASK | CAN_EFF_FLAG | CAN_RTR_FLAG` 精确过滤器，排除同数值扩展帧和 RTR；
  不订阅 error frame。启用 `SO_TIMESTAMPNS`，由 `recvmsg()` 辅助数据取得内核接收
  时间戳，并对 datagram 长度、DLC、ID flags 和 timestamp 完整性做防御性复核。
- 验证边界：`--can-receive COUNT --can-timeout-ms MS` 是有限板端门禁入口，使用总超时
  避免无效帧持续输入导致测试不退出。它直接生成/记录原始 `telemetry_record`，不投入
  M1 ring buffer，不实现 DBC/checksum/生产者--消费者/MQTT，因此没有提前进入 M3+ 或
  M5。
- 证据决定：x86_64 主机单测可以验证 socket 选项、辅助时间戳解析和错误注入，但不能
  代替 ARM 交叉编译、真实 i.MX6ULL bind/controller loopback、非目标 ID 过滤日志。
  后三者未运行时 M2 必须保持门禁未通过。

## D-015：M2 使用可迁移 Buildroot SDK 和显式 Linux feature 宏

- 日期：2026-08-29
- 状态：M2 已接受并经板端运行验证
- 决定：仓库只提交 `gateway/cmake/toolchains/imx6ull-buildroot.cmake`，通过
  `IMX6ULL_SDK_ROOT` 引用执行过 `relocate-sdk.sh` 的外部 Buildroot SDK；`ToolChain/`
  和厂商 SDK 源码目录保持 `.gitignore` 排除，不把 2.3 GiB 工具链或构建产物纳入源码
  可信源。编译定义同时使用 `_POSIX_C_SOURCE=200809L` 和 `_DEFAULT_SOURCE`，前者约束
  POSIX API，后者使目标 glibc 2.30 公开 `SO_TIMESTAMPNS` 等 Linux 接口。
- 证据：首次 ARM run `artifacts/20260829T131347+0800-m2-arm-cross/` 因目标头文件未公开
  `SO_TIMESTAMPNS` 保留为 FAIL；修正后的
  `artifacts/20260829T131442+0800-m2-arm-cross-final/` warning-clean PASS，并由新的普通
  主机及 ASan+UBSan run 各 9/9 回归。
- 限制：板端 Buildroot 修订为 `2020.02-g65177d4`，SDK 修订为
  `2020.02-gee85cab`。glibc 2.30、hard-float loader 和 ARMv7 ABI 一致只提供兼容线索；
  最终以 `artifacts/20260829T133148+0800-m2-board-loopback/` 运行该 SHA256 固定的
  `gatewayd`，动态加载和 M2 功能均通过，关闭了这项运行兼容性风险。

## D-016：M2 以真实 controller loopback 证据通过并冻结边界

- 日期：2026-08-29
- 状态：M2 已接受并通过
- 决定：M2 退出依据由 warning-clean 主机/ARM 构建、主机 9/9 普通与 ASan+UBSan、
  SHA256 固定的板端 binary、真实 i.MX6ULL controller loopback 及最终逐字节证据审计
  共同组成。板端三目标 ID、非目标过滤、DLC 拒绝和 `SO_TIMESTAMPNS` 提取均有原始
  日志，因此 M2 门禁通过。
- 恢复边界：经批准的测试结束后 `can0` 为 DOWN/STOPPED、loopback off；iproute2 没有
  通用“清空 bit timing”操作，500000 bit/s 参数在 DOWN 状态保留。该结果明确记录，
  不表述为完全恢复未配置状态，也不为清除参数扩大到 reboot/驱动重绑。
- 时间边界：板端 wall clock 未初始化，内核 timestamp 只能证明辅助数据提取、正值与
  顺序，不能转换成 UTC 正确性、时延或性能数字。
- 范围边界：controller loopback 不能替代物理 CAN、STM32、DBC、实际生产者--消费者、
  MQTT 或长期运行证据。M3-A 及后续功能没有开始，必须另行确认后按门禁推进。

## D-017：M3 跨 Windows、真实硬件和 Ubuntu 严格交接

- 日期：2026-08-29
- 状态：历史决定；当前状态由 D-019、D-020 更新
- 决定：M3 仍是唯一活动 Milestone，但 M3-A～M3-E 必须按证据顺序跨环境交接。
  Windows clone 独占 CubeMX/Keil Build 和 ST-Link 证据，真实断电硬件独占接线/电阻
  证据，Ubuntu/i.MX6ULL 独占物理 `candump` 和 `gatewayd` 证据。任一环境不能用源码
  审阅、假设值或另一环境的成功结果替代当前门禁。
- 当前影响：`artifacts/20260829T141730+0800-m3-preflight/` 已复核 M2 前置门禁，但
  Ubuntu 没有 CubeMX/Keil，仓库没有 `.ioc`/`.uvprojx`，所以 M3-A 为
  `NOT RUN - 需要用户在 Windows STM32CubeMX/Keil 中验证`。M3-B～M3-E 保持 `NOT RUN`；
  不为绕过门禁在 Ubuntu 手工伪造 CubeMX/Keil 工程，也不提前实现 M4 DBC。

## D-018：M3-A 在板卡时钟和引脚事实确认前停止

- 日期：2026-08-29
- 状态：历史 BLOCKED 记录；后续事实由 D-019 解除
- 依据：Windows 审计 `artifacts/20260829T144926+0800-m3a-preflight-windows/` 确认本机
  有 CubeMX/Keil/STM32F1 DFP，但仓库没有实际板卡照片、芯片/晶振丝印、原理图或明确
  板型，无法确认外部晶振来源/频率及 PB8/PB9 的板级可用性。项目规范记录的
  `STM32F103C8T6` 是项目所有者提供的型号，不足以推导具体板卡一定使用常见 8 MHz
  晶振，也不足以证明 PB8/PB9 未被板载器件占用。
- 决定：不得假定 “Blue Pill 8 MHz”，不得先创建虚构时钟的 `.ioc`。M3-A 的 CubeMX
  工程创建、Generate Code 和 Keil Build 全部保持 **NOT RUN**；获得照片、丝印、原理图、
  板卡资料或用户明确说明后，用新的唯一 run_id 重试。
- 影响：M3-B 不得实施；M3-C 等待全部断电的接线/电阻实测；M3-D 等待 M3-C 及烧录/
  `can0` 分别批准；M3-E 等待物理 `candump` 及至少 10 分钟运行批准。M3 总门禁不完成。

## D-019：STM32 改用 PA11/PA12，并按项目所有者实际现象关闭 M3-A～M3-D

- 日期：2026-08-30
- 状态：已接受，取代 M3 中原 PB8/PB9 方案及 D-018 的当前阻塞状态
- 决定：STM32 bxCAN 使用默认映射 PA11/CAN_RX、PA12/CAN_TX，不启用 CAN remap；
  PA11/PA12 不再用于 USB 数据通信。仓库工程、实际接线和项目所有者报告的物理运行
  均采用这一方案。
- 依据：项目所有者确认实际 MCU 为 STM32F103C8T6、外部晶振为 8 MHz；Keil Build
  `0 Error(s), 0 Warning(s)`；接线和终端已经核对；SoC 物理 `candump` 收到
  `0x100`/`0x101`/`0x102`，周期约 10/100/1000 ms、DLC 8、三个 byte 6 counter 独立
  递增、byte 7 XOR 正确，并连续运行10分钟现象正常。
- 证据边界：项目所有者明确选择简化验收，不补建 M3-A～M3-D 的新测试 artifact。
  文档可据此记录项目进度，但必须标注“项目所有者确认、原始日志未归档”，不得虚构
  Build、欧姆值、`candump` 行数、CAN error 数或可靠性结论。历史 preflight artifact
  仍保持当时 BLOCKED/NOT RUN 的含义。
- 影响：M3-A～M3-D 视为完成；STM32 侧不再增加功能。M3 总门禁仍等待 M3-E，M4 及
  后续不得开始。

## D-020：M3-E 需要最小长时接收能力，现有 M2 CLI 不足以直接验收

- 日期：2026-08-30
- 状态：历史计划；由 D-021 取消剩余门禁
- 依据：当前 `gatewayd --can-receive COUNT --can-timeout-ms MS` 把总超时上限限制为
  60000 ms，最终 `M2_CAN_SUMMARY` 只有总 attempts/accepted/reject/error 计数，没有
  按 CAN ID 的 frame 数或 Rolling Counter gap。
- 决定：先用现有入口做短时真实 CAN smoke test；随后只在 M3-E 范围内补齐至少10分钟
  的单次单调时钟运行和按 ID frame/counter-gap summary，再执行正式长时测试。不得在
  此步骤引入 DBC、解码、队列、MQTT 或任何 M4+ 功能。

## D-021：项目所有者以短时真实接收关闭 M3，并豁免 M3-E 长时门禁

- 日期：2026-08-30
- 状态：已接受，M3 已完成
- 依据：经批准将 `can0` 配置为500 kbit/s、loopback off 后，板端 `gatewayd` 完成两次
  1110帧真实接收；两次 summary 均为 `attempts=1110 accepted=1110`，timeout、所有
  reject、timestamp error 和 receive error 均为0。首次观察到一次瞬态 `error-warn`
  状态转换，随后恢复 ERROR-ACTIVE、tx/rx error counter 为0；干净复测期间项目所有者
  确认 CAN 状态和错误计数无新增异常。测试后 `can0` 恢复为 DOWN。
- 决定：项目所有者明确要求停止进一步测试，不增加长时/按 ID gap 功能，并取消原
  M3-E 连续10分钟门禁；以现有真实短时接收接受 M3 完成。
- 限制：当前 binary 的总超时上限仍为60000 ms，且没有按 ID gap summary；没有执行
  连续10分钟测试。因此 M3 完成不得描述成10分钟、稳定性、可靠性或性能 PASS，也不
  自动批准进入 M4。

## D-022：M4 使用 DBC + 共享黄金向量 + 定点静态解码布局

- 日期：2026-08-30
- 状态：首轮技术设计已接受；M4 完成判定由 D-023 撤销
- 决定：`protocol/vehicle.dbc` 是实验用自定义信号位布局、Intel 小端、缩放、偏移和
  单位的协议依据。Linux C 解码器不在接收热路径使用浮点，而把物理量保存为显式定点
  整数单位；32 字节解码区由 schema version、消息类型、counter、checksum、有效信号
  mask 和24字节消息 union 组成。
- 布局边界：不把 `uint8_t decoded_payload[32]` 强制转换为结构指针，统一通过 `memcpy`
  写入/读取，并以 `_Static_assert` 冻结各消息 signal payload 和总大小。checksum 或其他
  解码错误会清零解码区和 M4 状态位，但保留原始 CAN 数据及已有 timestamp 状态。
- 一致性方法：C 单测和独立 Python 标准库 DBC 检查器共同读取
  `protocol/test_vectors/vehicle_golden.csv` 的20条向量；除边界和错误路径外，C 测试按
  M3 固件规则遍历三类消息全部768种 counter。这样同时约束 DBC、黄金向量、C 布局和
  STM32 确定性生成规律，不依赖第三方运行库或代码生成器。
- 集成边界：M4 只提供静态 `decode_frame`/`decode_record` API，没有把解码调用接入
  ring buffer、生产者--消费者、MQTT 或 spool；实际线程链路仍属于 M5。M4 ARM binary
  只完成交叉构建，没有部署或板端执行；新的物理 CAN 解码明确为 `NOT RUN`。
- 证据：默认 warning-clean 和 ASan+UBSan 全量回归均为11/11 PASS，ARMv7 交叉构建
  PASS；对应 run 为 `artifacts/20260830T205736+0800-m4-host-final/`、
  `artifacts/20260830T205834+0800-m4-asan-ubsan/` 和
  `artifacts/20260830T205937+0800-m4-arm-cross/`。LeakSanitizer 仍为 `NOT RUN`。

## D-023：STM32 必须按 DBC 编码有物理意义的确定性模拟车况

- 日期：2026-08-30
- 状态：已接受，M4 门禁重新为 NOT MET
- 澄清：项目所有者明确 STM32 不需要真实传感器，但必须模拟有物理意义的车速、转速、
  油门等信号。旧固件让 byte 0～5 与 counter 同步递增，只证明报文字节确定且可解码；
  它会造成挡位每10 ms递增、物理量快速跳变和回绕，不能视为合理模拟车况。
- 决定：`protocol/vehicle.dbc` 继续作为唯一协议依据，Windows 侧不得修改 DBC 迁就固件。
  STM32 应以整数定点物理量生成确定性场景，再按 DBC factor/offset、Intel 小端布局编码；
  spare bits 清零，三个独立 Rolling Counter 和 byte 0～6 XOR 保持不变。
- 门禁影响：现有20条向量、768帧旧规律测试、主机/ASan/ARM PASS artifact 保持真实历史
  含义，但不再足以关闭 M4。Windows 提交真实 Keil Build 和 `candump` 依据后，Ubuntu
  必须以新 run 更新物理场景黄金向量并复测；在此之前不得进入 M5。

## D-024：以语义化主机模型和实物逐帧审计关闭 M4

- 日期：2026-08-31
- 状态：已接受，M4 门禁 MET
- 决定：保留 D-023 对 DBC 不可迁就固件的约束。STM32 在 `USER CODE` 区域使用整数
  定点生成60秒熄火/怠速/加速/巡航/减速/停车循环，按冻结 DBC 的 Intel 小端和
  factor/offset 编码，spare bits 清零，并继续维护三类独立 counter 与 XOR。
- 复核方法：共享黄金向量扩展为42条，其中31条取自实物捕获；C 测试重建完整6660帧
  场景，Python 检查器验证 DBC/向量，独立 candump 审计器对实物6000/600/60帧逐帧
  检查 payload、counter、XOR、spare bits 和前后 CAN 统计。
- 证据：`artifacts/20260831T112833+0800-m4-host-semantic-final/` 与
  `artifacts/20260831T111733+0800-m4-stm32-physical-final/` 均 PASS。项目所有者确认
  Keil 0 error/0 warning并已Download，但完整原始控制台输出未归档；当前 Windows
  sanitizer 因旧 MinGW 工具链限制为 `NOT RUN`。
- 边界：M4 关闭只证明自定义 DBC、静态解码器、STM32 语义模型和实物 CAN 输入一致。
  新增解码器尚未在 i.MX6ULL 实时路径运行，生产者--消费者属于 M5；M4 关闭不自动
  授权进入 M5，也不产生可靠性或性能结论。

## 本次规范冲突修正清单

| 原规范/状态 | 本次调整 | 修正位置 |
| --- | --- | --- |
| Ubuntu 未发现 STM32 GNU 工具链，被列为 STM32 待解决条件 | STM32 改由 Windows CubeMX + Keil + ST-Link 验证；Ubuntu 工具缺失不阻塞 Linux 侧 | `AGENTS.md`、`PROJECT_SPEC.md`、`OPEN_QUESTIONS.md` |
| STM32 开发框架、IDE、编译/烧录方式全部未知 | 工作流已确定；仅工具版本、工程文件、Clock/bit timing 和真实输出仍未知 | `PROJECT_SPEC.md`、`PLANS.md`、`HARDWARE.md` |
| M3 是一个整体阶段 | 拆为 M3-A 基础工程、M3-B 模拟 ECU、M3-C 断电检查、M3-D `candump`、M3-E `gatewayd` | `PLANS.md`、`TEST_PLAN.md` |
| CAN 收发器型号和终端状态未知 | STM32 侧确认为 TJA1050 + 120 Ω；i.MX6ULL 侧确认为板载 CAN 控制器 + TJA1042T/3 + 120 Ω + TVS + CANH/CANL 接口；禁止额外终端，仍以断电实测为门禁 | `PROJECT_SPEC.md`、`HARDWARE.md`、`OPEN_QUESTIONS.md` |
| 物理 CAN 可直接由 `gatewayd` 调试 | 强制先通过接线/电阻检查，再通过 `candump`，最后进入 `gatewayd` | `PLANS.md`、`TEST_PLAN.md` |
| 文档和少量注释以英文为主 | 新文档/人工注释默认中文，标识符和专业术语保留英文，旧材料渐进转换 | `AGENTS.md`、本轮更新文档 |
| STM32 中间产物只有通用 `*.o` 被忽略 | 增加 `Objects/`、`Listings/`、`*.d`、`*.axf` 忽略规则 | `.gitignore` |
| STM32 CAN 原计划使用 PB8/PB9 remap | 实际工程和接线改用 PA11/CAN_RX、PA12/CAN_TX 默认映射；禁用 USB 数据功能 | `PROJECT_SPEC.md`、`HARDWARE.md`、`PLANS.md` |
| M3-A～M3-D 等待完整 artifact | 项目所有者按真实 Build/接线/`candump` 现象简化验收；明确未归档原始日志 | `PLANS.md`、`milestones/M3.md`、`RESUME_TRACEABILITY.md` |
| M3-E 原计划连续10分钟并统计按 ID gap | 两次1110帧真实接收正常后，项目所有者取消剩余长时门禁并接受 M3 完成 | `PLANS.md`、`milestones/M3.md`、`TEST_PLAN.md` |
