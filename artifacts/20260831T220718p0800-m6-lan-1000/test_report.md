# M6 i.MX6ULL → Windows Broker 1000-batch 局域网实测

- Run ID：`20260831T220718p0800-m6-lan-1000`
- 源码提交：`e25ab9131ccaad2f4a71651fb0bd2d17f6f10794`
- Windows侧实测结论：**PASS_WITH_LIMITATIONS**
- M6项目门禁文档：**未更新，等待Ubuntu端复核原始证据**
- M7及后续功能：**未测试、未实现**

## 测试拓扑与边界

临时 Mosquitto 2.1.2 仅监听 Windows 有线地址 `192.168.50.1:18884`。唯一防火墙
规则精确允许远端 `192.168.50.2`；Windows无线、`0.0.0.0`、虚拟机和公有云不在链路
中。板端使用 `eth1=192.168.50.2/24`、`can0=500000 bit/s`、私有部署目录中的
gatewayd和libmosquitto 2.0.11。没有修改板端系统时间、CAN配置、系统libmosquitto、
Windows服务或仓库源码。

Topic为：

`vehicle/imx6ull-gateway-m6/telemetry/20260831T220718p0800-m6-lan-1000`

## 正式 subscriber 与 validator

- subscriber真实退出码：`0`；stderr：0字节；
- `subscriber.jsonl`：精确1000行、24554669字节，SHA256为
  `0645674698cf34f95be38dd2ada41d74eb3d672b08b02469c65a4f4fbb46383d`；
- validator退出码：`0`，状态：`PASS`；
- batch_seq严格为1..1000，missing/duplicate/reordered均为0；
- 1000批共115335条records，每批111..120条；
- 所有records按接收顺序展开后，gateway seq严格为1..115335，
  missing/duplicate/reordered均为0；
- schema、device_id、record_count、first_seq、last_seq、DLC、data和
  decoded_payload长度检查全部通过；
- CAN ID 0x100/0x101/0x102分别为103905/10391/1039条，DLC均为8。

独立 `payload_audit.json` 再次逐条计算了接收顺序，所有结构和顺序错误计数均为0。

## gatewayd与Broker对账

板端在收到停止信号后即时写入 `gateway_exit=0`。最终summary为：

```text
can_accepted=119083 decode_success=119083 queue_success=119082 queue_drop=0
queue_pop=119082 queue_count=0 mqtt_connect_success=1
publish_attempts=1033 publish_accepted=1033 puback_matched=1033
puback_unexpected=0 batches_acked=1033 records_acked=119082
last_batch_seq=1033 last_gateway_seq=119082 buffered=0 in_flight=0
mqtt_errors=0 reconnects=0
```

Broker原始日志独立得到：板端PUBLISH=1033、发给板端PUBACK=1033、转发给正式
subscriber的PUBLISH=1000、正式subscriber PUBACK=1000。gateway publish/PUBACK
统计与Broker完全一致。

subscriber在第1000批自动退出，到人工按Ctrl+C之间gateway又成功确认33批、3747条
records，因此gateway最终1033批/119082条大于subscriber文件的1000批/115335条；
该差异由同一Broker日志和gateway summary完整解释，不是subscriber缺失。

`can_accepted=119083`与`queue_success=119082`存在1帧差值，同时`queue_drop=0`；本次
证据没有记录该停止边界差值的独立原因，因此不作推测，也不将其描述为全链路每个CAN帧
均发布成功。正式subscriber已经保存的115335条gateway seq本身严格连续。

## CAN前后统计

前后均为`ERROR-ACTIVE`、500 kbit/s、berr-counter tx/rx=0。快照增量：

- RX packets：+119103；RX bytes：+952824；
- RX errors/dropped/overrun：均+0；
- restarted、bus-errors、arbit-lost、bus-off：均+0；
- error-warn和error-pass增量均为0；其累计值167和3是测试前已有值；
- TX packets/bytes/errors/dropped：均+0。

CAN接口快照覆盖范围比gateway进程运行边界更宽，RX packets增量比`can_accepted`多20；
没有独立证据定位这20帧的具体时间，故不推测原因。

## 清理

- 正式subscriber PID 36712和wrapper PID 28840均已退出；
- 临时Broker PID 27596已停止，Broker日志记录正常终止；
- TCP 18884最终listener_count=0；
- 既有Mosquitto Windows服务PID 33212保持运行，未被操作；
- 板端即时`pgrep`检查因命令不存在而退出127；稍后的`ps`与`/proc`只读补证显示
  `gatewayd_process_count=0`。

## 限制、失败尝试与NOT RUN

- 板端wall clock为1970年。因此 **absolute UTC/timestamp correctness = NOT RUN**；
  record timestamp不得用于正确UTC、端到端时延或性能结论。
- 板端到Windows的反向ICMP为0/4；Windows防火墙只明确放行TCP 18884。实际MQTT TCP
  连接、1000条subscriber payload及1033个PUBACK均成功，反向ICMP不作为MQTT门禁。
- Broker启动时没有退出码wrapper；停止后的`Process.ExitCode`未返回数值。因此Broker
  精确退出码为 **NOT AVAILABLE**，不能由空stderr或正常终止日志推测为0。
- v3外层`M6_COMMAND_EXIT`没有在v3返回时独立归档；较晚补证读到值1，但该变量无法与
  v3调用绑定。v3外层命令退出码为 **NOT AVAILABLE**；同目录即时gateway退出文件为0。
- 即时板端最终进程检查：**NOT RUN**（`pgrep`不存在）；稍后补证只说明补证时为0进程。
- 第一次板端交互命令会错误退出SSH，v2又错误地把反向ICMP作为硬门禁；两个失败目录
  均保留。正式数据只来自attempt3。
- 两次subscriber准备诊断在接收payload前被主动终止，退出-1；正式subscriber是第三次
  独立启动，退出0。
- 第一次Windows自动对账因生成文件格式错误产生两个假False；原文件保留，修正后的
  `acceptance_checks_v2.*`直接重读原始Broker日志，全部核心检查为True。
- 吞吐、性能、长期可靠性、正确UTC/时延、Broker断线、重连、spool、补传、恢复、
  去重、kill -9和epoll：**NOT RUN**。本run只证明M6局域网QoS 1功能链路。

Windows侧原始功能结果满足此前1000-batch验收条件；由于上述证据限制且尚未完成Ubuntu
端独立复核，本报告不修改或宣称项目M6总门禁为MET。
