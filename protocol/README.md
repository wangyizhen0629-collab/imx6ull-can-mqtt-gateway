# 自定义车辆遥测协议

本目录保存实验用自定义 DBC 和黄金向量，不是 OEM 或量产车辆协议。

## M4 冻结格式

`vehicle.dbc` 是信号位布局、Intel 小端、缩放、偏移和单位的协议依据。三类标准帧均为
DLC 8；byte 6 是各消息独立的模 256 `RollingCounter`，byte 7 是 byte 0～6 的 XOR。

| CAN ID | 信号 | 位布局 | DBC 物理量 | C 解码定点单位 |
| --- | --- | --- | --- | --- |
| `0x100` | `VehicleSpeed` | byte 0～1, LE | `raw * 0.01 km/h` | `vehicle_speed_centi_kph = raw` |
| `0x100` | `EngineSpeed` | byte 2～3, LE | `raw * 0.25 rpm` | `engine_speed_quarter_rpm = raw` |
| `0x100` | `ThrottlePosition` | byte 4 | `raw * 0.4 %` | `throttle_tenth_percent = raw * 4` |
| `0x100` | `Gear` | byte 5 | `raw` | `gear = raw` |
| `0x101` | `BatteryVoltage` | byte 0～1, LE | `raw * 0.001 V` | `battery_millivolt = raw` |
| `0x101` | `CoolantTemperature` | byte 2 | `raw - 40 degC` | `coolant_celsius = raw - 40` |
| `0x101` | `StateOfCharge` | byte 3 | `raw * 0.4 %` | `soc_tenth_percent = raw * 4` |
| `0x101` | `FaultFlags` | byte 4～5, LE | `raw` | `fault_flags = raw` |
| `0x102` | `Odometer` | byte 0～2, LE | `raw * 0.1 km` | `odometer_tenth_km = raw` |
| `0x102` | `DoorFlags` | byte 3 bit 0～3 | `raw` | `door_flags = raw` |
| `0x102` | `IgnitionState` | byte 4 bit 0～1 | `raw` | `ignition_state = raw` |

`test_vectors/vehicle_golden.csv` 是首轮人工向量，包含小端、缩放、偏移、位掩码、
信号最小/最大值、旧 STM32 counter 0/1/255 字节规律和错误路径。C 单元测试与独立的
DBC 检查脚本共同读取同一文件，避免为两个实现维护两套期望值。

STM32 M3 固件的 byte 0～5 生成规则是
`(base + rolling_counter + byte_index) modulo 256`，三个 base 分别为 `0x10`、`0x20`、
`0x30`。这些字节只是历史上可验证的确定性输入，不能解释为有物理意义的模拟车况。

2026-08-30，项目所有者进一步明确：STM32 必须在不接真实传感器的前提下生成合理、
确定性的模拟车速、转速、油门等物理信号，并依据本 DBC 的 factor/offset、Intel 小端
布局反向编码。Windows 侧修正时不得修改 DBC 来迁就固件；应使用整数定点计算，将未
定义的 spare bits 清零，并继续维护三个独立 counter 和 XOR。修正固件后必须替换/扩展
上述黄金向量并重新通过 M4；当前旧向量 PASS 不能单独关闭 M4。
