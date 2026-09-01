# 授权与执行边界

- 操作者明确要求继续争取`M9 MET`，并声明板端操作由操作者本人手动执行，Codex只提供命令指导。
- 操作者另行明确授权：仅按仓库已验证基线把`can0`恢复为bitrate 500000、loopback off、UP，
  允许修改本次CAN状态和波特率并保存前后统计。
- 禁止修改Broker、网络配置、STM32或其他系统条件；本run没有执行这些修改。
- 操作者确认：本次reboot后，在首次post-boot检查之前未手动执行gatewayd `start`、
  `restart`或向supervisor发送HUP。
- 本run不执行M10、性能、压力、长时或24小时测试。
