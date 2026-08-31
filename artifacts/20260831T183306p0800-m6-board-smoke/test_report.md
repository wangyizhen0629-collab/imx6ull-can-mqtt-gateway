# M6 i.MX6ULL → Windows Broker smoke artifact 审计

- Run ID：`20260831T183306p0800-m6-board-smoke`
- 源码提交：`7ab18cce43617000e8846998c0fd3aca8b67bdfb`
- 审计结论：**PASS_WITH_LIMITATIONS（仅已有 smoke 证据）**
- M6 总退出门禁：**NOT MET**

## 可由原始文件证明的事实

- Windows 到板端 `192.168.50.2` 的 ping 为4/4，0%丢包。
- 临时 Mosquitto 2.1.2 精确监听配置为 `192.168.50.1:18884`；Broker 日志记录来自
  `192.168.50.2` 的 gatewayd 连接。
- Broker 日志记录109次 gatewayd PUBLISH和109次发给gatewayd的PUBACK。
- gatewayd 最终 summary 为：`publish_attempts=109`、`publish_accepted=109`、
  `puback_matched=109`、`puback_unexpected=0`、`batches_acked=109`、
  `records_acked=12487`、`mqtt_errors=0`、`reconnects=0`。
- gatewayd 记录 `gateway_exit=0`，最终 `gateway_running=0`；Broker 日志记录正常终止，
  `final_listener_count=0`。
- subscriber 只保存1条 JSON batch：schema 为 `gateway.telemetry.v1`，device ID 为
  `imx6ull-gateway-m6`，`batch_seq=1`，含111条 records，gateway seq严格为1..111；
  CAN ID `0x100/0x101/0x102` 分别为100/10/1条，DLC均为8，data和decoded_payload
  长度均正确。修改后的 validator 在独立 run 中重放该文件并 PASS。

## 证据限制

- 板端日志显示1970年，板端 wall clock 未正确初始化。因此 **absolute UTC/timestamp
  correctness = NOT RUN**；record timestamp不能用于正确UTC、端到端时延或性能结论。
- `subscriber_exit.txt` 明确写有 `subscriber_exit=NOT_AVAILABLE`。因此 subscriber 精确
  退出码为 **NOT AVAILABLE / NOT RUN**，不能根据空 stderr、收到消息或DISCONNECT推测为0。
- subscriber 在收到首个 batch 后断开，只保存109个已确认 batch中的第1个；其余108个
  batch没有 subscriber payload，不能对其 gateway seq做独立重放验证。
- 只有 `can_after.txt`，没有 `can_before.txt`，所以 CAN error计数增量为 **NOT RUN**。
  after快照中的累计 error-warn/error-pass 不能解释成本次增量。
- gateway summary 中 `can_accepted=12488`、`queue_success=12487`、`queue_drop=0`；现有
  summary没有说明1条差值的具体分类，禁止推测原因。
- 本次只有109个已确认 batch，不是正式要求的1000-batch门禁；持续运行、吞吐、时延、
  可靠性以及M7重连/spool/恢复/去重均为 **NOT RUN**。

本次审计仅新增报告、manifest和校验清单，未覆盖、截断或修改原始 smoke 文件。
