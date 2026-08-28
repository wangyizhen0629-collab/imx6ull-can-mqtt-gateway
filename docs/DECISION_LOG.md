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
