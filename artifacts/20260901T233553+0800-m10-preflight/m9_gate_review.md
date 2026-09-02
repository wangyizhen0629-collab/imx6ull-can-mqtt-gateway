# M9前置门禁复核

- M9文档状态：`MET`。
- 基础run：`artifacts/20260901T204152+0800-m9-windows-board-gate-final/`。
- 基础run归档自检：`artifact_manifest_check.v4.txt`记录109项、0不匹配、PASS。
- 补充run：`artifacts/20260901T230215+0800-m9-manual-postboot-gate/`。
- 补充run归档自检：`artifact_manifest_check.txt`记录29项、0不匹配、PASS。
- 补充run结构化结果：新boot ID已取得；BusyBox init自动拉起supervisor为PASS；最终
  supervisor/child为1/1；child PID 9951超过60秒不变；binary SHA256为
  `6e8729417b3dc40c10a413459de5eca9be43ce58dfcc8a3b12e91f5c8d7ef958`。
- 基础run已证明受控restart、一次真实child SIGKILL恢复，以及目标BusyBox 1.31.1 ash
  隔离storm cooldown；补充run补齐真实reboot后init自动拉起和最终状态。

复核限制：在当前Ubuntu clone直接执行Windows生成的`artifact_manifest.sha256`会因文本
文件CRLF/LF checkout规范化而不匹配；M9目录未被`.gitattributes`标记为`-text`。这不被
记录为原始artifact损坏，也不伪装成跨clone逐字节PASS；门禁采用已归档的生成端自检、
结构化summary和关键事实交叉复核。后续证据目录应显式避免同类跨clone行尾歧义。

结论：M9 BusyBox进程监督范围的前置门禁为`MET`，允许本轮开始且只开始M10。

