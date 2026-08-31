# M4 STM32 实物 CAN 最终审计

结论：`PASS`。`candump_60s.log` 中恰有 6660 帧：`0x100` 6000 帧、
`0x101` 600 帧、`0x102` 60 帧。审计脚本逐帧重建 STM32 的 60 秒整数定点
车况模型，并校验 DBC Intel 小端布局、factor/offset、三类独立 Rolling Counter、
byte 0～6 XOR 及 spare bits，6660 帧的有效载荷差异均为 0。

周期统计分别为：`0x100` 平均 10.000 ms（9.926～10.080 ms），`0x101`
平均 99.997 ms（99.965～100.025 ms），`0x102` 平均 999.972 ms
（999.941～1000.006 ms）。这些是本次观测值，不是硬实时保证。

`can0` 接收计数从 109193 增至 115933，共增加 6740 包、53920 字节，字节数
严格等于 6740×8；比 candump 多出的 80 包发生在两次统计快照与 candump 命令窗口
之间，不能当作丢包。前后均为 `ERROR-ACTIVE`、500000 bit/s、berr-counter tx/rx
均为 0，所有已列错误计数增量为 0。

证据边界：原始目录名中的 1970 年是目标板时钟未初始化造成的，不代表实际采集时间；
这三份原始文件曾在 `tmp/` 中被重新上传，本目录是其非覆盖归档。捕获行为与所列源码
SHA256 完全一致，但没有固件镜像哈希，故不能声称源码与已烧录镜像具有密码学绑定。
用户确认真实 Keil Build 为 0 error、0 warning 且已 Download，但完整原始 Keil 控制台
输出未归档，因此只记作 owner confirmation。`0x102` 的物理 counter 仅覆盖 0～59，
不声称已物理观察到回绕；其边界由主机测试补充。

复核命令：

```text
python tools/protocol/check_stm32_candump.py \
  --candump artifacts/20260831T111733+0800-m4-stm32-physical-final/candump_60s.log \
  --can-before artifacts/20260831T111733+0800-m4-stm32-physical-final/can_before.txt \
  --can-after artifacts/20260831T111733+0800-m4-stm32-physical-final/can_after.txt
```
