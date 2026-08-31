# M6 Windows Broker smoke（失败保留）

结论：`FAIL`。配置语法检查通过，临时 Broker PID 8380 只监听
`192.168.50.1:18884`。subscriber 使用包含 run_id 的 topic，但 run_id 中的 `+0800`
被 MQTT subscription filter 解释为非法的嵌入式单层 wildcard，因此 subscriber 在
publish 前退出。`smoke_subscriber.stderr` 保存了原始错误。

publisher 为 `NOT RUN`，没有产生 CONNECT/PUBLISH/PUBACK smoke 结论，subscriber
payload 文件为0字节。失败后通过 `mosquitto_signal shutdown` 停止 Broker，并确认
TCP 18884 listener 数量为0。防火墙、网络和服务状态均未修改。

该失败属于测试 topic 构造错误，不是 Broker 监听失败。按不可覆盖证据规则，本目录
保持原样；重试必须使用不含 MQTT wildcard 字符的新 topic 和新的 artifact/run_id。
