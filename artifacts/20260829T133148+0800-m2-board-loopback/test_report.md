# M2 i.MX6ULL controller loopback 报告

## 结论

经用户明确批准后，项目所有者在真实 100ASK i.MX6ULL 上运行 SHA256 固定的 ARMv7
`gatewayd`。动态加载、三个目标 ID、非目标 ID 内核过滤、错误 DLC 拒绝、内核时间戳
提取和接口恢复均为 **PASS**。

## 实际结果

- `/tmp/gatewayd-m2 --version` 返回 `gatewayd 0.1.0-m1`，证明该动态链接 ARM binary
  可在目标 rootfs 启动。
- `can0` 从 DOWN/STOPPED 临时配置为 500000 bit/s controller loopback；运行时为
  ERROR-ACTIVE，CAN error 计数保持 0。
- 交错发送三个非目标帧及 `0x100`、`0x101`、`0x102` 后，`gatewayd` 只按顺序接受
  三个目标帧，payload 与发送值一致，summary 为 `accepted=3`、`timeouts=0`，所有
  rejection/timestamp/receive error 为 0。
- 三个 `kernel_timestamp_ns` 分别为 `57135391850767`、`57135415856100`、
  `57135439981100`，均为正数且递增。板端 wall clock 未初始化，因此这些值只证明
  `SO_TIMESTAMPNS` 提取与顺序，不证明真实 UTC 时间或 CAN 时延。
- 只发送 `0x123` 时没有 `M2_CAN_FRAME`，预期 timeout，`accepted=0`。
- 发送 `0x100` DLC 3 时记录 `reason=dlc`，`rejected_dlc=1`、`accepted=0`，随后按
  预期 timeout。
- 合计发送 8 帧、59 字节，driver TX error 与六类 CAN error 计数均为 0。
- 测试后恢复为 DOWN/STOPPED，loopback 标志消失；两个恢复命令返回 0。500000 bit
  timing 仍保留在关闭状态，这一限制在执行前已经说明，不能描述为完全恢复未配置状态。

## 证据完整性

原始未压缩 tar 保留在同名 `.tar` 文件，SHA256 为
`a538c92bc4aef201df3c8dd7069285d48c997af98ffd0378732b443086f54163`。最终主机审计
`artifacts/20260829T134148+0800-m2-final-audit/` 逐字节比较 18 个归档成员并重新判定
全部用例为 PASS。

controller loopback 不等于物理 CAN 或 STM32 证据；后者属于 M3，未执行。
