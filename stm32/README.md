# STM32F103C8T6 模拟 ECU

STM32 子项目是确定性 CAN 流量发生器，不是网关主要复杂度来源。M0 尚未实现固件；
M3-A/M3-B 将在 Windows 使用 STM32CubeMX 生成 Keil 工程，在 VS Code + Codex 中
维护业务源码，以 Keil Build 作为真实编译门禁，并由 Keil/ST-Link 完成烧录验证。

计划运行 bxCAN，重映射到 PB8/CAN_RX、PB9/CAN_TX，通过 TJA1050 模块接入
500 kbit/s 物理 CAN。只实现三类周期帧、Rolling Counter、XOR Checksum、确定性信号
和必要调试/错误统计；不引入 RTOS、USB 协议、GUI、网络栈、MQTT 或文件系统。

CubeMX 生成后，人工逻辑优先写入 `USER CODE BEGIN/END` 区域。仓库应保存 `.ioc`、
`Core/Inc`、`Core/Src`、必要 HAL Drivers 和 `.uvprojx`；`Objects/`、`Listings/`、
`*.o`、`*.d`、`*.axf` 由 `.gitignore` 排除。烧录前必须取得明确批准。
