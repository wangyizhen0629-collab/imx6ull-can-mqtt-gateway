# M6 libmosquitto QoS 1 最终集成证据

结论：**PASS（Ubuntu x86_64 loopback 功能门禁）**。

项目所有者明确批准后，测试从已下载到 `/tmp`、未系统安装的 Ubuntu 包运行
Mosquitto/libmosquitto 2.0.11。Broker 只监听 `127.0.0.1:18884`、允许匿名测试连接、
禁用持久化；没有修改系统服务、`/etc` 或网络接口。测试后以 SIGINT 停止 Broker并确认
端口不再监听。

## 1000-batch 结果

- publisher：`batches=1000`、`records=1000`、`publish_attempts=1000`、
  `publish_accepted=1000`、`puback_matched=1000`、`puback_unexpected=0`；
- publisher 的 `last_batch_seq=1000`、`last_gateway_seq=1000`、`failed=0`；
- subscriber 保存1000行/336027字节原始 JSON；validator 确认 batch_seq 和 gateway seq
  都严格为1～1000，missing=0、duplicate=0、reordered=0；
- Broker 原始日志独立统计 final topic 上 Received PUBLISH=1000、发送给 publisher 的
  PUBACK=1000、转发给 subscriber 的 PUBLISH=1000。

每条测试记录立即形成一个 batch，用于快速验证1000次 publish/PUBACK 状态机；该次数
不是吞吐或时延测量。另一个独立用例使用100 ms测试 interval 和最大256条 batch，在
只有一条记录、没有第二条输入的情况下由 idle poll 触发，105.236 ms后收到匹配 PUBACK。
该数字只证明 timer 路径生效，不是性能指标；生产默认仍为1000 ms。

## 边界

本 run 使用同一 Ubuntu 主机的 loopback Broker/publisher/subscriber，不是局域网跨主机、
i.MX6ULL、物理 CAN 或 `gatewayd --run-mqtt` 的板端证据。没有测试 Broker 断线、重连、
spool、`kill -9`恢复、去重或 epoll；这些属于 M7/M8。
