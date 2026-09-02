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

`test_vectors/vehicle_golden.csv` 当前有42条向量：31条来自语义化 STM32 实物捕获的
代表帧，8条静态边界向量，另有 checksum、未知 ID 和错误 DLC 三条错误路径。C 单元
测试与独立 DBC 检查脚本共同读取同一文件，避免为两个实现维护两套期望值。旧的20条
原始字节递增向量及768帧测试只保留在历史 artifact 中，不再代表当前固件模型。

## STM32 确定性车况模型

STM32 不接传感器，按60秒循环生成以下有明确含义的模拟车况：

| 时间 | 状态 | 主要变化 |
| ---: | --- | --- |
| 0～3 s | 熄火停放 | 车速/转速/油门为0，点火关闭，驾驶员门打开 |
| 3～5 s | 启动怠速 | 点火运行，800 rpm，车门关闭 |
| 5～20 s | 加速 | 车速线性升至60 km/h，油门32%，挡位依次1～4 |
| 20～40 s | 巡航 | 60 km/h、约2200 rpm、18%油门、4挡 |
| 40～55 s | 减速 | 车速线性降至0，油门归零并逐级降挡 |
| 55～58 s | 停车怠速 | 0 km/h、800 rpm |
| 58～60 s | 熄火 | 点火关闭，转速归零，驾驶员门打开 |

所有计算采用整数定点；再严格按 DBC factor/offset 和 Intel 小端布局编码。未定义 spare
bits 清零，三类消息分别维护模256 counter，byte 7 始终是 byte 0～6 XOR。里程只在
车辆运动时按固定点余数累计，并在完整循环中从123456.7 km增加到123457.2 km。

实物捕获和逐帧审计位于
`artifacts/20260831T111733+0800-m4-stm32-physical-final/`。其中6660帧与上述模型逐帧
一致；该证据是离线 CAN/协议审计，不代表 `gatewayd` 已在 i.MX6ULL 实时调用解码器。

## M10压力profile

`m10_traffic_profiles.json`冻结111/500/1000帧/s三档输入。111档保持100/10/1帧/s；
500档为450/45/5帧/s，1000档为900/90/10帧/s。两个压力档用100-slot整数超帧调度，
不改变ID、DLC、DBC、counter或XOR合同；60秒车况模型仍按10 ms状态时钟推进。

精确调度、发送失败语义、Keil target和正式计数下限见
`docs/milestones/M10_STM32_PROFILE_DESIGN.md`。这些是已冻结的测试输入定义，不是已经
完成的烧录或真实速率证据；Windows三个Keil target的构建准备证据已PASS，但仍必须以
烧录后的短candump证明物理profile。
