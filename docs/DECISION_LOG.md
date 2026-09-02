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
- 状态：已接受并由真实等价门禁关闭M8
- 原因：目标 libmosquitto 版本和 external loop API 尚未确认。
- 影响：M6/M7基线已建立；M8已确认目标库API、实现reactor并通过真实i.MX6ULL/Windows
  Broker等价门禁。只允许使用带“单次受控功能验证”限定的epoll表述。

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

## D-025：M5 使用单生产者、单消费者和可注入 mock sink

- 日期：2026-08-31
- 状态：设计已接受并实现；本条记录的是板端证据到达前状态，后由 D-026 关闭门禁
- 决定：`gateway_pipeline` 固定为一个 CAN producer 和一个 consumer。producer 通过
  有上界的 receive 回调取得 M2 `telemetry_record`，在接收热路径调用 M4 解码器后再
  使用 M1 ring buffer 有界入队；consumer 排空队列并调用 M5 mock sink。实际 CAN
  回调每100 ms返回一次以检查退出，不在热路径执行 MQTT、spool 或复杂序列化。
- seq/丢弃语义：每个通过 M2 原始帧校验的记录都会推进 gateway seq；checksum/解码
  拒绝和满队列丢弃因此形成可观察的 seq gap。满队列继续采用 D-012 冻结的“有界等待
  后丢弃新记录”，queue timeout、CAN receive error 和 decode error 分开统计。
- 退出语义：`SIGINT`/`SIGTERM` 继续由 self-pipe 唤醒主线程；主线程 request stop 并
  close/broadcast，producer 最迟在有界 receive 返回后退出，consumer 先 drain 已入队
  记录再返回 CLOSED，所有线程 join 后才销毁 queue、stats 和 lifecycle。
- mock sink：只验证 timestamp/checksum/decode 状态并统计 consumed、seq gap、非单调和
  无效记录。`--mock-sink-delay-ms` 只用于 M5 故意慢消费者，不是 MQTT batch 或未来
  sink 配置。
- 证据：主机 warning-clean 与 ASan+UBSan 全量回归均12/12 PASS；主机111帧/s定时合成
  输入 queue drop 0，容量4的故意过载和真实 `SIGTERM` 用例通过；Buildroot GCC 7.5.0
  ARMv7 warning-clean 交叉构建通过。
- 当时边界：设计与主机测试完成时尚没有目标板证据，因此当时 M5 为 NOT MET。项目
  所有者此后自行完成板端测试并提供原始日志；最终门禁判定见 D-026。D-025 的设计
  语义保持不变。

## D-026：以项目所有者提供的板端原始日志关闭 M5

- 日期：2026-08-31
- 状态：已接受；M5 门禁 MET
- 决定：归档项目所有者从 i.MX6ULL 拉回的基准和过载原始日志，并以交叉构建 binary
  SHA256 `567079d01f4fb1e682a959cd01bac3709e4062f42c1f18903596dc47181d0a01`
  绑定板端运行对象。证据位于
  `artifacts/20260831T132341+0800-m5-board-owner-final/`。
- 基准判定：capacity 1024、push timeout 50 ms、sink delay 0；3694条物理 CAN 输入全部
  decode、入队、pop 和消费，queue drop、sink gap、非单调和无效记录均为0。
- 过载判定：capacity 4、push timeout 0、sink delay 20 ms；6532次入队尝试中2970次
  成功、3561次按策略丢弃、1次因退出时 queue closed，`pop=consumed=2970`、
  `sink_gap=3561`、high-watermark 4/4。两次日志均记录 signal 15，并随后输出线程 join
  后的 summary，满足 M5 graceful-shutdown 门禁。
- timeout 解释：项目所有者确认 STM32 在两个进程启动后才中途开启；305/75次 receive
  timeout 是此前无输入阶段的100 ms空闲 poll，不是 queue drop。不能把进程的完整
  64/69秒描述为持续111帧/s。
- 证据限制：未归档 `can_before/after`、正确 UTC 和 shell `wait` 精确退出码，因此不作
  CAN 错误增量、持续运行、吞吐、时延或可靠性结论。M5 在此停止；本决定不授权 M6。

## D-027：M6 使用 libmosquitto 单 in-flight QoS 1 batch 基线

- 日期：2026-08-31
- 状态：设计已接受并在 Ubuntu x86_64 实现
- 决定：使用经过实际编译/运行验证的 libmosquitto，不手写 MQTT 协议。M6 使用
  MQTT 3.1.1、QoS 1、clean session，并把 libmosquitto in-flight上限设为1。每次 publish
  保存 MID；只有 publish callback 返回相同 MID 才算成功并推进 batch_seq、已确认记录数
  和 last acknowledged gateway seq。unexpected MID 和网络错误单独统计。
- batch格式：JSON schema固定为 `gateway.telemetry.v1`，包含 device ID、batch_seq、
  record_count、first/last seq和记录数组。记录保留原始 CAN data、内核 timestamp、
  status flags和32字节解码区的十六进制表示。batch内 seq严格递增，但允许上游丢弃形成
  gap；subscriber按 `device_id + seq` 验证 unique/missing/duplicate。
- 有界性：生产默认 batch interval为1000 ms，batch最多256条、payload buffer为固定
  128 KiB。consumer增加可选 timed pop/idle callback；MQTT路径每50 ms维护库loop并检查
  到期 batch，使稀疏输入不必等待下一条记录。M5 mock sink不设置idle callback，继续
  使用原阻塞pop和close后drain语义。
- 配置：新增 `mqtt_ack_timeout_ms`，默认5000 ms、范围100～60000 ms；password不能在
  username为空时单独存在。真实凭据继续只从配置/CLI取得且日志脱敏。
- 失败边界：M6不重连、不写spool、不补传。连接或PUBACK失败会停止pipeline，未确认
  batch不得计为成功。断线恢复、原始重复处理、持久化和seq恢复属于M7；本阶段不调用
  `mosquitto_socket/loop_read/loop_write/loop_misc`组合的external reactor，epoll仍属M8。

## D-028：M6 主机 loopback 基线通过，但总门禁保持 NOT MET

- 日期：2026-08-31
- 状态：已接受；M6实现完成、退出门禁 NOT MET
- 主机证据：`artifacts/20260831T135537+0800-m6-host-final/` warning-clean且沙箱外
  CTest 13/13 PASS；`artifacts/20260831T135603+0800-m6-asan-ubsan/` 的ASan+UBSan也是
  13/13 PASS，LeakSanitizer为 `NOT RUN`。
- MQTT证据：经项目所有者明确批准，
  `artifacts/20260831T135630+0800-m6-mqtt-final/` 使用实际Mosquitto/libmosquitto 2.0.11
  和仅监听 `127.0.0.1:18884` 的临时Broker。publisher完成1000次attempt、1000次库接受
  和1000个匹配PUBACK，unexpected=0；subscriber得到1～1000连续batch_seq/gateway seq，
  missing/duplicate/reordered均为0。Broker日志也分别计到1000次PUBLISH和PUBACK。
- timer证据：同一run以100 ms测试interval和1条记录验证idle tick会发送稀疏batch；
  105.236 ms只作为路径功能值，不作为产品时延或性能指标。
- 未满足项：上述通信全部发生在同一Ubuntu主机loopback，不是测试计划规定的局域网
  跨主机链路。`artifacts/20260831T135759+0800-m6-arm-dependency-audit/` 又确认Buildroot
  SDK缺少目标 `mosquitto.h`、库和pkg-config元数据，所以M6 ARM构建、部署和真实
  i.MX6ULL物理CAN → MQTT均为 `NOT RUN`。
- 门禁决定：不能用host loopback替代LAN/目标证据，因此M6总门禁保持 `NOT MET`，也不
  产生“完整CAN--MQTT网关”、板端QoS、吞吐、时延或可靠性结论。M7不得开始。
- 收尾复核：`artifacts/20260831T140625+0800-m6-close-audit/` 确认M6专项单测、
  subscriber重放、归档hash和范围审计通过；临时Broker端口已无listener。
  这只是证据一致性收尾，不改变上述 `NOT MET` 决定。

## D-029：真实i.MX6ULL物理CAN到Windows LAN Broker证据关闭M6

- 日期：2026-09-01
- 状态：已接受；M6门禁MET
- 决定：以SHA256固定的ARMv7 `gatewayd`和私有libmosquitto 2.0.11在真实i.MX6ULL
  运行，物理CAN输入发布到Windows Mosquitto 2.1.2。正式subscriber保存1000批、
  115335条连续记录；batch seq 1～1000、gateway seq 1～115335，missing/duplicate/
  reordered均为0。gateway和Broker原始日志对账为1033次publish及1033次匹配PUBACK，
  unexpected和MQTT error均为0。
- 独立复核：`artifacts/20260901T090837+0800-m6-lan-gate-review/`验证原始
  `manifest.sha256` 131/131、strict validator 8/8、subscriber重放、Broker/gateway及
  CAN前后计数，判定PASS_WITH_LIMITATIONS。
- 限制：板端wall clock仍为1970；停止边界`can_accepted`比`queue_success`多1帧的原因
  没有独立证据；不产生正确UTC、时延、吞吐或长期可靠性结论。D-028的host阶段
  `NOT MET`保持历史事实，但M6总门禁由本决定更新为`MET`。

## D-030：M7采用逐记录耐久spool、保守cursor恢复和QoS 1去重边界

- 日期：2026-09-01
- 状态：设计与离线实现已接受；M7总门禁后续由D-031更新为MET
- data格式：固定80字节entry，使用magic/version/length、CRC32和显式little-endian
  `telemetry_record`载荷；逐条append后`fdatasync`。文件为append-only并持有单写者
  `flock`，启用64-bit file offset。
- state格式：固定40字节，保存CRC、已确认offset/seq和next batch。更新顺序是临时文件
  写入、`fdatasync`、`rename`、父目录`fsync`；只有匹配当前MID的PUBACK后才推进。
- 恢复策略：部分尾部或最后一条损坏记录截断到最后有效entry；内部损坏拒绝启动。
  state缺失/损坏/越界时从offset 0安全重放。该保守策略允许raw duplicate，禁止把未确认
  数据当成成功；gateway seq始终从data最后有效记录继续。
- 断线策略：durable模式将Broker不可用视为可恢复transport状态，继续同步spool并按
  `mqtt_reconnect_interval_ms`重试；磁盘/格式错误是fatal。每次重连重建clean-session
  libmosquitto客户端，仍限制一个in-flight batch。
- 容量边界：当前不做compaction和静默drop；ENOSPC或sync失败采用fail-stop并单独计数。
  目标介质、容量阈值、寿命和后续回收策略必须由真实部署条件决定，不能在M7证据中
  推测。
- 当时验证边界：Ubuntu普通/ASan+UBSan全量16/16及ARMv7 warning-clean构建已通过；
  Windows Broker断线/恢复、subscriber原始抓取、实际`kill -9`及目标持久介质当时均为
  `NOT RUN`。该历史判断已由D-031的跨主机证据更新；D-030本身不授权M8。

## D-031：以真实Windows Broker、i.MX6ULL ext4 spool和一次SIGKILL关闭M7

- 日期：2026-09-01
- 状态：已接受；M7总门禁MET
- 决定：采用`artifacts/20260901T105414+0800-m7-lan-recovery-gate2/`作为M7跨主机退出
  证据。目标使用`/dev/root` ext4下的专用`/var/lib`目录；`/tmp`只部署binary/library，
  不作为持久化介质。部署的gateway和libmosquitto均在板端重新计算SHA256。
- 崩溃边界：Broker离线且spool有29445条pending时，只对核实的PID 1706执行一次
  `kill -9`。命令返回0、wait137，data/state大小和hash不变；同binary、配置和spool
  重启后按seq4865～34309补传，gateway seq没有复用。
- 重连边界：PID1926又经历一次运行中Broker停止/恢复并记录`reconnects=1`；新增物理CAN
  记录最终ACK到seq35644。只有匹配PUBACK时cursor才推进，正常summary的unexpected
  PUBACK、queue drop、spool pending/corruption/error均为0。
- 损坏边界：部分尾部只截断无效4字节；内部magic损坏在MQTT connect前拒绝启动；state
  magic损坏安全回退到offset0并重放35644条。该回退故意允许raw duplicate，禁止跳过
  未确认数据。
- 完整性：最终raw batch/record为281/71288，raw duplicate35644，unique gateway seq
  35644，missing0、effective duplicate0。Broker gateway PUBLISH/PUBACK和subscriber
  PUBLISH/PUBACK均为281/281；板端manifest本地复核115/115。
- 限制：SIGKILL phase没有最终queue summary；正常退出phase为queue drop0且seq验证无
  缺失。CAN最终为ERROR-WARNING、berr rx102，目标时钟仍为1970。正确UTC、真实掉电、
  性能、介质寿命、容量/compaction及长期可靠性保持`NOT RUN`。
- 影响：M7改为`MET`，只允许声称一次受控的板端断线/进程崩溃/损坏恢复功能门禁通过。
  本决定不批准、不实现也不测试M8 external-loop/epoll或M9/M10。

## D-032：目标libmosquitto API满足条件，M8采用epoll external reactor

- 日期：2026-09-01
- 状态：已接受；M8总门禁`MET`
- API依据：`artifacts/20260901T125544+0800-m8-preflight/`对与M7板端运行库SHA256
  相同的ARMv7 libmosquitto 2.0.11核对头文件和动态符号，`mosquitto_socket`、
  `mosquitto_loop_read`、`mosquitto_loop_write`、`mosquitto_loop_misc`以及
  `mosquitto_want_write`均5/5存在。
- 决定：新增独立reactor，由epoll统一监控MQTT socket、eventfd和timerfd；eventfd承载
  本地操作唤醒，timerfd周期调用`loop_misc`，socket按`want_write`动态订阅EPOLLOUT。
  M7的单in-flight、匹配PUBACK后推进cursor、断线重建client和spool恢复语义不改。
- 离线证据：最终Ubuntu run为warning 0、全量CTest 17/17、M8专项2/2、ASan/UBSan 2/2；
  ARMv7 binary无RPATH/RUNPATH并引用含`mosquitto_connect_async`在内的6个API。早期ARM
  查找/RPATH失败以及两次Windows实现失败均保留，不能改写成PASS。
- 真实证据：`artifacts/20260901T170636+0800-m8-windows-reactor-final-gate/`在真实i.MX6ULL、
  物理CAN、ext4 spool和Windows Broker上通过断线重连、一次SIGKILL、同spool恢复、state
  安全重放、reactor计数、优雅退出和严格validator。323个raw batch、51523条raw record、
  24089条raw duplicate去重后为1～27434连续序号，missing0、effective duplicate0。
- 影响：M8关闭为`MET`；可保留受限epoll external-reactor表述。正确UTC、掉电、性能、
  时延、CPU/RSS和长期可靠性不因此成立。M9不得自动开始。

## D-033：重连门禁必须控制subscriber首次捕获顺序，且不得修饰失败validator

- 日期：2026-09-01
- 状态：已接受
- 原因：第三个Windows run的gateway在无持久化Broker重启后先于clean-session subscriber
  连接，4个batch已获Broker PUBACK但未被subscriber捕获；后来state重放虽补齐unique集合，
  first-seen顺序仍使仓库validator失败。
- 决定：失败JSONL保持原顺序，禁止排序或放宽validator。最终run使用合法的20秒测试重连
  间隔，在一次async timeout后启动Broker，并用Broker日志证明subscriber连接/订阅早于
  gateway下一次自动重连；生产默认值和源码不因测试对齐而改变。
- 影响：第三个run保持FAIL；只有全新spool、完整重跑且validator exit0的第四个run用于
  M8 `MET`结论。

## D-034：M9采用BusyBox inittab respawn与前台节流supervisor

- 日期：2026-09-01
- 状态：已接受实现；M9总门禁`NOT MET`
- 前提：目标系统已确认使用BusyBox 1.31.1且无systemd；M8门禁已由
  `artifacts/20260901T175107+0800-m9-preflight/`复核为`MET`。
- 决定：把`null::respawn:/etc/init.d/gatewayd supervise`合并进目标inittab，由BusyBox
  init在sysinit/wait之后运行唯一前台supervisor。supervisor管理唯一gatewayd子进程，
  提供HUP受控restart、临时disabled stop/start、TERM转发、PID核验及快速失败阈值后的
  cooldown。不使用systemd，也不同时注册rcS `S??gatewayd`入口。
- 主机依据：warning-clean和ASan+UBSan全量CTest均18/18，M9专项1/1；独立BusyBox ash
  trace实际覆盖异常退出42重拉、PID替换、stop/start和3次快速失败后2秒cooldown。
- 目标依据：ARMv7 warning-clean构建、ELF/解释器/依赖和无RPATH/RUNPATH审计PASS；但
  真实i.MX6ULL部署、`/etc`、BusyBox 1.31.1 init、开机/restart/异常恢复均`NOT RUN`。
- 影响：部署方案和可执行测试骨架已经冻结，但没有目标板开机与异常拉起证据前，M9不得
  标为`MET`，对应简历表述仍不可用。M10不得开始。

## D-035：M9目标续跑以构建产物身份失败为停止点

- 日期：2026-09-01
- 状态：已接受；M9总门禁仍为`NOT MET`
- 只读事实：`artifacts/20260901T182509+0800-m9-windows-board-gate/`通过Windows既有
  目标路径确认PID 1为链接`libbusybox.so.1.31.1`的standalone `/sbin/init`；目标库含
  `respawn`和`reloading /etc/inittab`字符串。目标wall clock仍为1970。
- 阻塞事实：要求的Ubuntu `build/m9-arm-cross/gateway/gatewayd`无法用Windows现有SSH
  认证读取；Windows clone和目标板也没有预期M9 SHA文件。因此没有重新计算部署输入
  SHA256，禁止把预期值或旧M8 binary当作本次输入。
- 决定：按用户授权中的binary不可用停止规则，在非系统staging、`/etc`备份/安装、init
  HUP/reboot和所有进程信号前停止。结束只读审计证明inittab hash未变、M9 staging不
  存在、supervisor/gatewayd为0/0且无遗留测试进程；零/零不等于最终一/一门禁PASS。
- 影响：部署、开机、restart、SIGKILL恢复、fake storm cooldown和最终服务状态全部
  `NOT RUN`。后续必须先安全取得并实算匹配的M9 binary，再以全新artifact重试；M10
  不得开始。

## D-036：M9板端部分门禁通过，但reboot后不可达阻止关闭总门禁

- 日期：2026-09-01
- 状态：已接受；M9总门禁仍为`NOT MET`
- 输入决定：最终run `artifacts/20260901T204152+0800-m9-windows-board-gate-final/`只在
  板端重新计算104860-byte M9 binary为预期SHA256，并确认ARMv7 hard-float、动态解释器、
  NEEDED及无RPATH后才进入目标写入；匹配的私有libmosquitto也按M8固定SHA复核。
- 部署决定：保持系统库不变，把binary/lib放入`/opt/gatewayd`，在ext4本轮目录保存
  inittab和原文件不存在标记，只修改四个授权`/etc`文件。PID 1 HUP后按cmdline标识
  supervisor，并以exe+SHA+完整cmdline+PPID标识真实gatewayd；不能只按comm计数，因为
  目标shell supervisor的comm同样是`gatewayd`。
- 已通过事实：HUP启动后精确1/1稳定；一次核验子进程SIGKILL由同一supervisor拉起不同
  PID；受控restart更换子PID；目标BusyBox 1.31.1 ash隔离fake测试证明3次快速失败后
  cooldown、窗口内无第4次启动并恢复，且无遗留测试进程。
- 未通过事实：reboot前持久化marker和boot ID已保存，唯一一次reboot命令已发出；随后
  48次SSH探测均exit255，无法读取新boot ID。不得把命令发出或断线当成真实开机成功，
  因而开机自动启动、最终1/1、post-boot CAN/Broker及回滚为`NOT RUN`。
- 影响：目标登录恢复前不得修改网络或重复reboot；应先只读取得新boot ID和最终身份，
  必要时按已保存回滚脚本恢复。本run不关闭M9，M10不得开始。

## D-037：手动post-boot补证关闭M9 BusyBox进程监督门禁

- 日期：2026-09-01
- 状态：已接受；M9总门禁为`MET`，M10仍未开始
- 补充证据：`artifacts/20260901T230215+0800-m9-manual-postboot-gate/`由操作者手动执行板端
  命令。新boot ID与基础run不同；操作者确认首次检查前未人工start/restart/HUP。唯一
  supervisor PID 337的PPID为1，cmdline与inittab respawn入口一致，因此BusyBox init
  自动启动supervisor为PASS。
- 外部条件：post-boot `can0`初始为DOWN/STOPPED，真实child不能保持；该观察不伪装成
  PASS，也不把CAN状态武断写成唯一退出原因。操作者新增明确授权后只恢复仓库既有
  500000 bit/s、loopback off、UP基线；没有修改Broker、网络配置或STM32。
- 最终事实：受控start exit0；5秒和超过60秒检查均status exit0，supervisor/child精确
  1/1，child PID 9951不变、PPID 337、exe/cmdline/SHA和固定libmosquitto映射正确，其他
  gatewayd和测试进程为0，CAN保持UP/ERROR-ACTIVE且berr 0/0。
- 组合判定：本run补齐开机init和最终状态；D-036基础run已经通过restart换PID、一次
  SIGKILL恢复和目标BusyBox ash隔离storm cooldown。六项门禁均有真实证据，故M9关闭为
  `MET`。该决定不证明CAN持久自动配置、Broker交付、正确UTC、完整无人值守产品ready、
  性能或可靠性，也不授权开始M10。

## D-038：M10采用原始/proc差值与真实场景硬门禁，缺少板端证据时保持NOT MET

- 日期：2026-09-02
- 状态：已接受实现；M10总门禁`NOT MET`
- 前提：`artifacts/20260901T233553+0800-m10-preflight/`复核M9组合门禁为`MET`。M9的
  Windows文本artifact在Ubuntu checkout会因CRLF/LF规范化导致直接hash不匹配；本次只按
  已归档生成端manifest自检和结构化关键事实放行，并为全部M10 artifact增加`-text`。
- 指标决定：目标只采集原始`/proc/<pid>/stat`、`/proc/<pid>/status`、`/proc/stat`和
  `/proc/uptime`。Ubuntu按同一PID/starttime的相邻tick差计算CPU总系统容量占比；CPU、
  VmRSS、VmHWM报告平均、nearest-rank P95和最大值。采集器拒绝覆盖，PID缺失、身份不符、
  读取错误和PID变化都不得静默忽略。
- 场景决定：四个run互不替代，只接受`environment=imx6ull-physical`。500/1000帧/s各
  至少1800秒；Broker中断恰好20轮且每轮至少300秒；111帧/s基准至少86400秒。每秒指标
  必须覆盖完整时长，至少`duration+1`个样本，最大间隔不超过2.5秒。三类CAN ID分别
  报告帧数/gap，总数必须达到速率乘时长并与最终MQTT unique record相等；CAN error、
  queue drop、MQTT missing/effective duplicate和进程退出为0才允许单场景PASS。QoS 1
  raw duplicate单独报告且允许存在。CPU/RSS只报告，不虚构未冻结的产品阈值。
- 离线依据：主机warning-clean、沙箱外全量20/20、最终M10专项2/2；ASan+UBSan全量20/20
  和最终M10专项2/2；ARMv7 warning-clean/ELF/无RPATH均PASS。合成86401行CSV只验证门禁
  算法，不是24小时实测。LeakSanitizer为`NOT RUN`。
- 未满足事实：当前Ubuntu会话没有真实目标板、STM32、Windows Broker或subscriber路径。
  500/1000帧/s、20轮断网、板端指标和24小时测试全部`NOT RUN`，且没有改变任何外部状态。
- 影响：工具实现完成不等于性能/稳定性通过。M10总门禁保持`NOT MET`，相关简历表述不可用；
  本轮停止在M10。

## D-039：M10冻结整数STM32压力profile并改用RelWithDebInfo正式输入

- 日期：2026-09-02
- 状态：Windows准备提交`06eaf8e`已push并PASS；等待Ubuntu复核，M10仍为`NOT MET`
- 输入决定：项目所有者明确选择推荐方案。三个编译期Keil target分别为111帧/s的
  100/10/1、500帧/s的450/45/5和1000帧/s的900/90/10；压力档使用100-slot整数超帧，
  slot周期为2 ms/1 ms。60秒车况仍按10 ms状态时钟推进，不随高频采样加速。
- 失败语义：只有CAN TXOK才推进该ID counter。发送失败只累计failure，不重试或突发补发；
  主循环错过slot只累计missed并按单调时钟跳过。正式门禁要求failure/missed均为0，且由
  candump独立验证速率、ID序列、counter、DLC、XOR和CAN前后统计。
- Windows依据：M10分析器无硬件回归7/7 PASS；三个ARMCC 5.06u6 Keil target全量rebuild
  均0 error/0 warning，产品SHA见
  `artifacts/20260902T094824+0800-m10-windows-profile-prep2/`。第一次捕获脚本失败run保留，
  第二次uVision异步产品检查竞态的原始`MISSING`及完成后纠正证据同时保留。
- binary决定：正式性能输入不使用现有Debug SHA；Ubuntu拉取本准备提交后生成、验证并冻结
  `RelWithDebInfo` ARM binary的新SHA。该项当前`NOT RUN`。
- 影响：Windows准备PASS只允许进入Ubuntu复核停止点，不授权烧录、目标/Broker控制、
  短预演或长时间测试；真实四场景未执行，M10保持`NOT MET`。

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
