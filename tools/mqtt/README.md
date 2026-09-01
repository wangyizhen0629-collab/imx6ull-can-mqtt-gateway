# MQTT 工具

M6 使用 libmosquitto，不手写 MQTT 协议。`mosquitto-m6-local.conf` 只监听
`127.0.0.1:18884`、允许匿名测试连接且禁用持久化；它必须由测试命令显式启动，不能
安装为系统服务。启停任何 Broker 前仍须取得明确批准。

`validate_m6_batches.py` 读取 `mosquitto_sub -F %p` 保存的逐行 JSON，支持每个 batch
包含一条或多条 record。工具验证 schema、device ID、batch 元数据、DLC/十六进制字段
长度；协议整数使用严格 JSON integer 语义，`true`/`false` 不得冒充整数。`batch_seq`
必须从1开始严格连续到 `expected-batches`。所有 batch 的 records 会
按接收顺序展开，gateway `seq` 必须全局严格连续为 `1..last_gateway_seq`，因此 record
总数不要求等于 batch 数。M6 的1000-batch gate 还必须同时核对 publisher 侧
`publish_attempts`、`publish_accepted`、匹配 PUBACK 和 unexpected PUBACK 统计。

`test_validate_m6_batches.py` 是不访问网络、不启动 Broker 的 M6 回归测试，覆盖跨 batch
多记录连续性、gap/duplicate 拒绝，以及布尔值不能充当整数的边界。

该M6 validator不测试断线、重连、spool、去重恢复或epoll。M7恢复validator和显式
恢复驱动负责恢复语义；M8在同一驱动上增加external-loop reactor计数断言。所有会启停
Broker或发送进程信号的测试仍须先取得明确批准。
