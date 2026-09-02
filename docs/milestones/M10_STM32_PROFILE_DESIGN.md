# M10 STM32压力profile冻结设计

## 决定与范围

项目所有者于2026-09-02确认采用本文件推荐方案。该确认只冻结准备代码和后续被测输入，
不等于烧录、板端/Broker控制、CAN状态修改或长时间测试授权。

- STM32 profile采用111、500、1000帧/s三个编译期Keil target；
- 500/1000档采用整数每ID速率，不使用Windows sleep或平均估算；
- M10 gateway被测binary选择`RelWithDebInfo`，由Ubuntu在拉取本准备提交后重新构建、
  warning-clean/回归/ELF/RPATH复核并给出新SHA；现有Debug SHA
  `7bb1d7299eac43d5a7a9b8f52981652c6ed3e3f3b29567ff74a1abc5f2b3edef`不作为正式M10
  性能输入。

## 冻结profile

| Keil target | 编译定义 | 调度 | `0x100` | `0x101` | `0x102` | 总速率 |
| --- | --- | --- | ---: | ---: | ---: | ---: |
| `M10_111` | `ECU_TRAFFIC_PROFILE=111` | 既有10/100/1000 ms独立周期 | 100 | 10 | 1 | 111帧/s |
| `M10_500` | `ECU_TRAFFIC_PROFILE=500` | 100-slot超帧，每slot 2 ms | 450 | 45 | 5 | 500帧/s |
| `M10_1000` | `ECU_TRAFFIC_PROFILE=1000` | 100-slot超帧，每slot 1 ms | 900 | 90 | 10 | 1000帧/s |

两个压力档的每个超帧均为90个`0x100`、9个`0x101`和1个`0x102`。slot 9、19、…、89
发送`0x101`，slot 99发送`0x102`，其他slot发送`0x100`。500档超帧为200 ms，1000档
超帧为100 ms；没有浮点、RTOS、运行时切档或突发补发。

按最小正式时间窗计算的精确下限为：

| 场景 | `0x100` | `0x101` | `0x102` | 合计 |
| --- | ---: | ---: | ---: | ---: |
| 500帧/s × 1800 s | 810000 | 81000 | 9000 | 900000 |
| 1000帧/s × 1800 s | 1620000 | 162000 | 18000 | 1800000 |
| 111帧/s × 86400 s | 8640000 | 864000 | 86400 | 9590400 |

正式run若使用更长的整数`duration_seconds`，验收下限随实际冻结时间增加，不能继续沿用
表中最小值。

## 业务语义和失败行为

- 三类报文继续使用标准11-bit ID、DLC 8、既有DBC编码、独立模256 Rolling Counter和
  byte 0～6 XOR；不会把压力profile描述成新的车辆协议。
- 111档保持既有60秒场景和逐帧语义。压力档仍按10 ms状态时钟推进同一60秒场景；增加
  的`0x100`帧是当前确定性状态的更高频采样，不加速车辆物理场景。
- `CAN_SendMessage()`只有收到真实TXOK才算成功。成功后才推进对应ID counter及
  `tx_success_count`；失败时增加`tx_failure_count`，不推进counter、不重试、不突发补发。
- 主循环错过slot时按单调HAL tick跳过旧slot，增加`ecu_stress_missed_slot_count`，不追赶
  发送。下一slot继续原100-slot相位，因此真实candump会出现速率、间隔或序列失败。
- Keil短测和正式run结束时必须保存三个message的success/failure计数及
  `ecu_stress_missed_slot_count`；failure或missed任一非0即FAIL，不得只凭平均速率通过。

## 构建和选择

`MDK-ARM/imx6ull-can-mqtt-gateway.uvprojx`含三个独立target和输出目录。每次烧录前必须对
所选target执行完整rebuild，保存Keil版本、target名、0 error/0 warning原始输出、下载
记录和生成固件SHA256。禁止在同一binary运行时切换profile；profile身份由target、编译
定义、固件SHA和短candump四方绑定。

`python -B tools/stm32/generate_m10_keil_targets.py --project \
stm32/firmware/imx6ull-can-mqtt-gateway/MDK-ARM/imx6ull-can-mqtt-gateway.uvprojx --check`
只复核三个target配置一致性，不执行Keil编译。Keil未真实运行时只能写`NOT RUN`。

Windows准备run `artifacts/20260902T094824+0800-m10-windows-profile-prep2/`已使用ARMCC
5.06u6对三个target执行完整rebuild，均0 error/0 warning，并记录六个`.axf/.hex`的
SHA256。该结果只证明当前源码可构建；构建产物未提交，也不能替代烧录后candump。

## candump独立分析

`tools/protocol/analyze_m10_candump.py`读取冻结JSON合同并支持111/500/1000三档，输出：

- 总数、每ID计数和实测持续时间/速率；
- 每ID Rolling Counter gap、XOR、DLC及`0x102` spare bits；
- 压力档100-slot ID序列、总/每ID间隔和gross interval；
- CAN error frame、意外/extended/RTR ID；
- `can_before/after`的bitrate、ERROR-ACTIVE、berr和RX/error统计差值。

分析器拒绝覆盖既有JSON。正式run仍需把其结果与gateway、MQTT、spool和`/proc`证据汇入
`gateway.m10.run.v1`，并由Ubuntu执行总validator；单独candump PASS不能关闭任一场景。

## 后续停止点

Windows准备提交push后立即停止。Ubuntu必须先拉取并复核diff、全量warning-clean/CTest、
分析器回归，生成并验证`RelWithDebInfo` ARM binary及新SHA。Ubuntu书面放行前不得Keil
烧录或开始120秒预演；外部状态和长时间测试仍需另行按清单第3节精确授权。
