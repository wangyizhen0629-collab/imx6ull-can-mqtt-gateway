# Windows Codex：M4 所需 STM32 语义化模拟信号修正

请先 `git pull`，完整阅读根目录 `AGENTS.md`、`docs/PROJECT_SPEC.md`、`docs/PLANS.md`、
`protocol/README.md`、`protocol/vehicle.dbc` 和当前 STM32 工程。当前只修正 M4 关闭所需的
STM32 模拟信号生成/编码，不得实现 M5 或后续功能。

## 目标

STM32 不连接真实传感器，作为确定性模拟 ECU 生成有物理意义的车速、转速、油门、
挡位、电源和车身状态。`protocol/vehicle.dbc` 是唯一协议依据；禁止修改 DBC 来迁就
固件。旧的 `base + counter + byte_index` 原始字节递增算法必须替换。

## 编码约束

- `0x100` 仍为100 Hz：
  - `VehicleSpeed` raw = 车速 `km/h * 100`，byte 0～1 Intel 小端；
  - `EngineSpeed` raw = 转速 `rpm * 4`，byte 2～3 Intel 小端；
  - `ThrottlePosition` physical = raw × 0.4%，byte 4；
  - `Gear` 放 byte 5，模拟值限制在合理挡位范围，例如0～6。
- `0x101` 仍为10 Hz：
  - `BatteryVoltage` raw = mV，byte 0～1 Intel 小端；
  - `CoolantTemperature` raw = 摄氏度 + 40，byte 2；
  - `StateOfCharge` physical = raw × 0.4%，byte 3；
  - `FaultFlags` 放 byte 4～5 Intel 小端。
- `0x102` 仍为1 Hz：
  - `Odometer` raw = 0.1 km 计数，byte 0～2 Intel 小端；
  - `DoorFlags` 放 byte 3 bit 0～3，byte 3 高4位清零；
  - `IgnitionState` 放 byte 4 bit 0～1，byte 4 高6位清零；
  - byte 5 是未定义 spare，必须清零。
- 三类消息 byte 6 各自维护模256 `RollingCounter`；byte 7 始终为 byte 0～6 XOR。
- 使用整数定点运算，避免浮点和动态分配。只有 `HAL_CAN_AddTxMessage()` 返回 `HAL_OK`
  后才推进对应 counter 和发送成功状态。

## 确定性场景

设计一个简单、可复现、可解释的周期场景，例如静止/点火、加速、匀速、减速/停车；
车速、转速、油门和挡位之间必须合理，慢速电源/车身信号也必须有明确规则。将周期、
每段持续时间、整数步进、边界和回绕规则写入中文文档或人工维护注释，并给出至少：

- 每类消息首帧；
- 场景转换边界帧；
- 最大/最小物理值；
- counter 0、1、255 及回绕；
- 每个样例的原始8字节、物理期望值和 XOR。

这些样例将由 Ubuntu 侧更新为新的 M4 黄金向量，因此不能只给截图或口头结论。

## 工程和证据边界

- 优先修改 CubeMX `USER CODE BEGIN/END` 区域，避免无必要修改自动生成区。
- 不引入 RTOS、USB 协议、TCP/IP、MQTT、文件系统、GUI 或复杂 bootloader。
- 使用 Windows Keil 执行真实 Build，保存工具版本、命令/操作时间和完整 Build 输出；
  未运行时必须标记 `NOT RUN - 需要用户在 Windows Keil 中验证`。
- 未经项目所有者另行批准，不得烧录、修改目标板/CAN 状态或执行长时间测试。
- 每次实际测试使用新的 `artifacts/<run_id>/`；不得覆盖现有 M3/M4 artifact。
- 提交 STM32 源码、场景说明、样例向量和真实证据后 push。不要把 M4 标记为完成；
  Ubuntu 侧还必须据此更新黄金向量并重新运行 DBC/C 解码测试。
