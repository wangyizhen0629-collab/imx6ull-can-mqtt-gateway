# 测试与支持工具

- `board/`：只读板端信息采集和后续板端辅助工具。
- `mqtt/`：后续 PC Broker/subscriber/序列校验工具。
- `can/`：后续 CAN 帧生成和证据采集工具。
- `metrics/`：M10 BusyBox `/proc` 采样、离线报告和四类真实板端场景门禁校验工具。
- `protocol/`：DBC/M4实物抓包检查及M10三档candump速率、序列和CAN统计分析。
- `stm32/`：生成/核对M10三个Keil编译期profile target的准备工具；不执行编译或烧录。

M0 只创建了只读板端信息采集脚本。
