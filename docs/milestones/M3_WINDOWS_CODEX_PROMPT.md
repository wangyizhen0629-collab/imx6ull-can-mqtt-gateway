# M3 Windows Codex 交接提示词

将下面整段提示词交给位于 **Windows 主机同一 Git 仓库 clone** 中的 Codex。开始前应
先把包含 `artifacts/20260829T141730+0800-m3-preflight/` 和本文件的变更同步到 Windows
clone，避免形成第二份独立 STM32 源码。

```text
你正在 Windows 主机上的 imx6ull-can-mqtt-gateway Git clone 中工作。只执行当前项目
M3 的 Windows 部分 M3-A 和 M3-B；不得实现 M4 或任何后续功能，也不得烧录固件、修改
i.MX6ULL can0、连接/上电硬件或推测硬件测试结果。

开始时完整读取仓库根目录 AGENTS.md、docs/PROJECT_SPEC.md、docs/PLANS.md、
docs/TEST_PLAN.md、docs/HARDWARE.md、docs/milestones/M2.md、docs/milestones/M3.md、
docs/DECISION_LOG.md、docs/OPEN_QUESTIONS.md、docs/RESUME_TRACEABILITY.md。检查 git status，
保留用户已有变更；确认仓库包含 M3 preflight。先复核 M2 门禁，但不得重做或改写 M2
证据。

严格按以下顺序工作：

1. M3-A 前置审计
   - 记录 Windows 版本、STM32CubeMX 版本、Keil MDK/UV4 版本、STM32F1 Device Family
     Pack 版本、ST-Link 工具版本。
   - 确认实际 MCU 是 STM32F103C8T6，确认实际板卡晶振频率/来源和 PB8/PB9 可用性。
     晶振或板卡事实没有照片、丝印、原理图或用户明确提供的依据时，不得假设常见
     “Blue Pill 8 MHz”；停止并向用户询问。
   - 每次测试创建唯一 artifacts/<run_id>/。失败 run 原样保留，重试使用新 run_id；
     不覆盖、截断或删除已有 artifact。

2. M3-A CubeMX/Keil 基础工程
   - 用 STM32CubeMX 为 STM32F103C8T6 新建工程，放在 stm32/firmware/ 下；Toolchain/IDE
     选择 MDK-ARM。保存 .ioc，并 Generate Code 生成 Keil .uvprojx。
   - 配置真实 Clock Tree，记录 SYSCLK、PCLK1、APB1 prescaler 和 CAN kernel clock。
   - 配置 bxCAN Normal mode，标准 11-bit Classic CAN，PB8/CAN_RX、PB9/CAN_TX 和正确
     CAN remap。检查生成代码中 remap 和 GPIO 初始化确实存在。
   - 用公式 bitrate = PCLK1 / (prescaler * (1 + BS1 + BS2)) 计算恰好 500000 bit/s，
     记录 prescaler、SJW、BS1、BS2、总 TQ、实际 bitrate、误差和 sample point。参数必须
     基于上一步真实 PCLK1，不得照抄未经验证的常见值。
   - 在 Keil 中真实 Build。保存完整原始 Build Output、退出/错误/警告计数、工具版本、
     .ioc/.uvprojx 和相关源码 SHA256 到唯一 M3-A artifact。只有真实 Build 0 error 后
     才能把 M3-A 标记 PASS；否则记录 FAIL 或 NOT RUN，不能进入 M3-B。

3. 只有 M3-A 真实 PASS 后才实施 M3-B
   - STM32 只作为最小确定性 CAN 流量发生器/模拟 ECU。不要引入 RTOS、USB 协议、GUI、
     TCP/IP、MQTT、文件系统、复杂 bootloader 或 M4 DBC/解码器。
   - 优先把人工逻辑放在 CubeMX USER CODE BEGIN/END 区域；如使用独立 ecu_traffic.c/.h，
     让生成文件只在 USER CODE 区调用它，并确保再次 Generate Code 不会覆盖业务逻辑。
   - 启动 bxCAN，以标准数据帧、DLC 8 周期发送：0x100 每 10 ms（100 Hz）、0x101 每
     100 ms（10 Hz）、0x102 每 1000 ms（1 Hz）。调度必须基于单调 HAL tick、处理
     uint32_t wraparound，并避免用阻塞 delay 造成三个周期漂移。
   - 三个 ID 各自维护 uint8_t Rolling Counter，放在 byte 6，模 256；byte 7 必须等于
     byte 0～6 的 XOR。bytes 0～5 使用简单、可重复、可由计数器/发送步数推导的确定性
     变化。把每个字节的精确公式、端序、范围以及“发送邮箱忙/发送失败时 counter 是否
     推进”的语义写入 M3 记录，但不要创建 vehicle.dbc 或 M4 黄金向量。
   - 检查 HAL_CAN_Start() 和 HAL_CAN_AddTxMessage() 返回值，保留最小发送成功/失败
     计数；不要增加无关调试框架。确保三个独立 counter 只按已定义的成功发送语义推进。
   - 再次在 Keil 中真实 Build，保存新的唯一 M3-B artifact；完整保留原始 Build Output、
     工具版本、源文件 checksum、命令/操作步骤、开始结束时间、PASS/FAIL 和限制。

4. 证据和文档
   - Keil 的 Objects/、Listings/、*.o、*.d、*.axf 不得提交。提交/同步 .ioc、.uvprojx、
     Core/Inc、Core/Src、必要 Drivers 和业务源码；检查没有密钥、真实局域网地址或生成
     中间产物。
   - 每个 artifact 至少包含 manifest.json、summary.json、test_report.md、原始 Keil
     Build 日志、tool_versions.txt、source_sha256.txt；JSON 必须可解析，日志不得手工
     改写为成功。
   - 更新 docs/PLANS.md、docs/DECISION_LOG.md、docs/OPEN_QUESTIONS.md、
     docs/RESUME_TRACEABILITY.md 和 docs/milestones/M3.md，只写 M3-A/M3-B 的真实状态。
     不得把 M3 总门禁标记为完成，不得把 Keil Build 描述为烧录或物理 CAN 成功。
   - M3-C、M3-D、M3-E 全部写 NOT RUN，原因分别是：等待全部断电的接线/电阻实测；
     等待 M3-C 通过及用户对烧录和 can0 修改的分别批准；等待 M3-D 物理 candump 通过
     及用户对至少 10 分钟运行的批准。

完成 M3-A/M3-B 后立即停止，报告修改文件、两个门禁结果、artifact 路径、尚未执行项
和需要同步回 Ubuntu clone 的内容。不要自行烧录、操作硬件或继续 M3-C/M3-D/M3-E。
```
