# M2 最终证据审计

本 run 修正第一次审计把 UID/GID 差异误作内容差异、并被 `tee` 掩盖退出码的问题。
验证脚本直接返回状态，并把 tar 解包到本 run 的独立目录，对 18 个原始成员逐文件执行
字节比较；所有内容一致。

二进制 checksum、板端汇总字段、动态加载、目标 ID 顺序/payload/正数递增 timestamp、
非目标过滤、DLC 拒绝、CAN 状态和恢复结果全部重新检查并 **PASS**。详细输出见
`verification.log`。

限制保持不变：板端 wall clock 未初始化；恢复后 bitrate 500000 仍在 DOWN 状态保留；
本结果只是 M2 controller loopback，不是 M3 物理 CAN/STM32 证据。
