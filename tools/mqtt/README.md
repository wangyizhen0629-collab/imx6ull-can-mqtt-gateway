# MQTT 工具

M6 使用 libmosquitto，不手写 MQTT 协议。`mosquitto-m6-local.conf` 只监听
`127.0.0.1:18884`、允许匿名测试连接且禁用持久化；它必须由测试命令显式启动，不能
安装为系统服务。启停任何 Broker 前仍须取得明确批准。

`validate_m6_batches.py` 读取 `mosquitto_sub -F %p` 保存的逐行 JSON，支持每个 batch
包含一条或多条 record。工具验证 schema、device ID、batch 元数据、DLC/十六进制字段
长度；`batch_seq` 必须从1开始严格连续到 `expected-batches`。所有 batch 的 records 会
按接收顺序展开，gateway `seq` 必须全局严格连续为 `1..last_gateway_seq`，因此 record
总数不要求等于 batch 数。M6 的1000-batch gate 还必须同时核对 publisher 侧
`publish_attempts`、`publish_accepted`、匹配 PUBACK 和 unexpected PUBACK 统计。

该工具不测试断线、重连、spool、去重恢复或 epoll；这些仍属于 M7/M8。
