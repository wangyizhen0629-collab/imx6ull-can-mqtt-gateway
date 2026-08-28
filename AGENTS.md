# 仓库规则

- 以 `docs/PROJECT_SPEC.md` 作为项目范围和架构依据，以 `docs/PLANS.md`
  作为阶段门禁依据。
- 每次只推进一个 Milestone；当前阶段没有真实证据并通过门禁前，不得进入下一阶段。
- 禁止虚构编译、烧录、硬件、运行时间、可靠性或性能结果。无法执行的检查必须写明
  `NOT RUN`、原因和所需条件。
- 每次测试使用唯一的 `artifacts/<run_id>/`。未经用户明确批准，不得替换、截断或
  删除已有证据。
- 修改目标板网络/CAN 状态、`/etc`、init 配置、进程、Broker 状态、固件、依赖，
  或执行长时间测试前，必须先获得用户批准。
- Windows 主机负责 STM32CubeMX、Keil MDK、ST-Link 编译/烧录门禁；Ubuntu
  虚拟机负责 `gatewayd`、ARM 交叉编译和 Linux 侧测试。Ubuntu 缺少
  `arm-none-eabi-gcc` 不得阻塞 Linux 侧 Milestone，也不得为此强行安装工具链。
- Windows 与 Ubuntu 使用同一个 Git 仓库的不同 clone；仓库源码是唯一可信源，
  禁止维护多份互相独立的 STM32 源码。
- 修改 CubeMX 工程时优先使用 `USER CODE BEGIN/END` 区域，避免无必要改动自动生成区。
- STM32 仅作为确定性 CAN 流量发生器/模拟 ECU，不引入 RTOS、USB 协议、GUI、
  TCP/IP、MQTT、文件系统或复杂 bootloader。
- 不使用 systemd、公有云或手写 MQTT 协议实现；不为简历关键词强行引入技术。
- 密钥和真实局域网地址不得进入源码；运行参数来自配置文件或命令行。
- 自定义测试 DBC 不得描述成 OEM 或量产车辆协议。
- 项目文档和人工维护的源码注释默认使用中文；代码标识符、文件名、Linux API、
  MQTT topic 和协议字段名保持英文。注释解释设计原因、并发约束和边界条件，禁止
  无价值的逐行翻译式注释。
- 构建产物放在源码树外的 `build/` 或被 `.gitignore` 排除；Keil 的 `Objects/`、
  `Listings/`、`*.o`、`*.d`、`*.axf` 不得提交。
