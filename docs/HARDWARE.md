# CAN 硬件拓扑与验证清单

## 必须区分的四个层次

- **CAN 控制器**负责生成和接收 CAN 帧。STM32F103 内部是 bxCAN；i.MX6ULL 的 CAN
  控制器由 Linux 驱动暴露。
- **CAN 收发器**负责在控制器 TX/RX 逻辑电平与差分 CAN 总线之间转换。STM32 侧使用
  外置 TJA1050 模块；i.MX6ULL 开发板侧使用已经集成的 TJA1042T/3。
- **CANH/CANL 物理总线**是差分导线，还必须连接公共 GND 并正确终端匹配。
- **SocketCAN `can0`**是 Linux 网络接口抽象，不是物理线、CAN 控制器或收发器。

禁止把 MCU 的 CAN_TX/CAN_RX 直接连接到另一块板的 CANH/CANL。

## 已确认硬件与最终拓扑

项目所有者确认 CAN 物理层所需硬件已经具备：

- STM32F103C8T6 侧使用外置 TJA1050 模块，该模块带 120 Ω 终端；
- i.MX6ULL 开发板已经配备完整 CAN 模块，包含 CAN 控制器、TJA1042T/3 收发器、
  120 Ω 终端、TVS 保护和 CANH/CANL 接口；
- STM32 侧 TJA1050 模块与 i.MX6ULL 板载 CAN 模块分别位于两节点总线的物理端点。

```text
STM32 PB9 / CAN_TX -> STM32 侧 TJA1050 TXD
STM32 PB8 / CAN_RX <- STM32 侧 TJA1050 RXD

STM32 侧 TJA1050 CANH（120 Ω） <-> i.MX6ULL 板载 CANH 接口
STM32 侧 TJA1050 CANL          <-> i.MX6ULL 板载 CANL 接口
STM32 侧 TJA1050 GND           <-> i.MX6ULL GND

i.MX6ULL 板载 CAN 模块：
CANH/CANL 接口 + TVS 保护 + 120 Ω 终端 + TJA1042T/3 + CAN 控制器
```

设计上已经有两个 120 Ω，**禁止再额外并联 120 Ω**。i.MX6ULL 侧收发器、终端、TVS
和 CANH/CANL 接口均已具备，不需要再接第二只外置 TJA1050；但已确认的硬件配置不能
代替实际接线后的断电测量。

## 终端电阻测试口径

M3-C 上电前必须让两块开发板、STM32 侧 TJA1050 模块及 i.MX6ULL 板载 CAN 模块全部
断电，再用万用表测量 CANH--CANL：

| 实测现象 | 可能含义 | 处理 |
| --- | --- | --- |
| 接近 60 Ω | 两个 120 Ω 并联，符合两节点预期 | 记录数值后继续其他断电检查 |
| 接近 120 Ω | 可能只有一个终端实际生效 | 停止上电并排查模块/接线 |
| 接近 40 Ω | 可能有三个 120 Ω 并联 | 停止上电，检查是否误接了额外外置终端 |
| 明显偏离上述范围 | 可能有接线、接触或电阻值问题 | 停止上电并定位原因 |

必须保存实际数值和测试记录，禁止仅凭已知配置推定测量结果。i.MX6ULL 板载 120 Ω
已确认，不再作为未知项；若实测接近 40 Ω，应排查线束或外部连接是否误加第三终端。

## STM32 约束与 Windows 工作流

- MCU：STM32F103C8T6；计划 CAN Remap 为 PB8/CAN_RX、PB9/CAN_TX。
- STM32CubeMX 负责 Clock、CAN、Remap 和 Keil 工程生成；Keil MDK 负责真实编译、
  调试和 ST-Link 烧录；Ubuntu 不承担 STM32 编译门禁。
- Codex 修改生成工程时优先放在 `USER CODE BEGIN/END` 区域，不无故修改 CubeMX
  自动生成区。
- 运行时不启用 STM32 USB CDC/DFU；USB 可用于供电或与外部烧录/调试设备配合。
- 根据实际 APB1 时钟计算：

  `bitrate = PCLK1 / (prescaler * (1 + BS1 + BS2))`

  同步段为 1 个 time quantum。必须记录 SJW、sample point、晶振/Clock Tree、APB1、
  prescaler、BS1 和 BS2；当前尚未选择或验证具体 timing 参数。

## M3 安全上电顺序

1. M3-A 在 Windows 保存 `.ioc` 并取得真实 Keil Build 结果。
2. M3-B 完成最小模拟 ECU 逻辑并再次通过 Keil Build。
3. M3-C 全部断电，核对 STM32 侧模块供电/逻辑电平/pinout、两端 CANH/CANL/GND
   和终端实测值；i.MX6ULL 侧直接使用板载 CANH/CANL 接口。
4. 只有等效电阻与接线合理后才上电；烧录 STM32 和修改 `can0` 前分别取得批准。
5. M3-D 先用 `candump can0` 验证真实报文，不能跳过它直接调试 `gatewayd`。
6. M3-E 才允许 `gatewayd` 使用物理 CAN；先低速观察状态，再达到 111 帧/s 基准。

## 当前已知与未验证边界

已知：STM32 侧为带 120 Ω 的 TJA1050 模块；i.MX6ULL 侧为完整板载 CAN 模块，使用
TJA1042T/3，并已包含 CAN 控制器、120 Ω、TVS 和 CANH/CANL 接口。项目不再缺少
i.MX6ULL 侧收发器、终端或保护电路。

尚未验证：实际 CANH--CANL 电阻、STM32 侧模块供电与逻辑电平兼容性、实际接线、
STM32 Clock Tree/bit timing/PB8-PB9 Remap、Keil Build/烧录、关闭 loopback 后的
`candump` 以及 10 分钟物理总线运行。本轮未连接、测量或修改任何硬件。
