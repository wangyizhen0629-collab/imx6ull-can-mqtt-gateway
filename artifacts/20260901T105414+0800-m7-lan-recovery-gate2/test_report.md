# M7 Windows LAN / i.MX6ULL 恢复门禁报告

## 结论

- Run：`20260901T105414+0800-m7-lan-recovery-gate2`
- 源码提交：`fefd07558f944f7cc1b2527730bae75ddd891be5`
- M6 前置门禁：`MET`
- M7 退出门禁：`MET`
- M8/M9/M10：`NOT RUN`，未实现、未测试、未批准

本次在真实 i.MX6ULL、物理 CAN 输入、目标板 ext4 持久目录和 Windows Mosquitto
Broker 上完成了在线基线、Broker 断线积压、一次实际 `kill -9`、同 binary/配置/spool
重启补传、同进程再次断线重连、尾部损坏、内部损坏拒绝及 state/cursor 损坏安全回退。
最终 subscriber 严格验证得到 `missing=0`、`effective duplicate=0`，因此满足 M7 的功能
退出条件。该结论不外推为掉电、性能、寿命或长期可靠性结论。

## 身份、部署与介质

- Windows Broker：Mosquitto 2.1.2，专用端口 18884，匿名、无持久化；配置和日志的
  提交副本已脱敏，原始副本保存在 Git 忽略的 `private_raw/`。
- `gatewayd`：`/tmp/m7/bin/gatewayd`，本次板端实算 SHA256 为
  `9c4efe5c12f9797e8eda03ada8c6b2162ff0225aa7680c7ac199afdc45382b25`。
  该值与历史记录相同是本次重新计算所得，不是根据历史值推测。
- `libmosquitto`：链接名 `/tmp/m7/lib/libmosquitto.so.1`，实际文件
  `libmosquitto.so.2.0.11`，本次板端实算 SHA256 为
  `b32c8ac4defb2b2920fba2e42f263869508c42e3c1719440db37ffc8d8c2f636`。
- `file`/`readelf`/`ldd` 证明 ARMv7 EABI5 hard-float，运行时确实解析到专用
  `/tmp/m7/lib/libmosquitto.so.1`，没有缺失动态库。
- spool 位于 `/var/lib/gatewayd-m7-test-20260901T105414`，挂载为可写
  `/dev/root` ext4、`data=ordered`；最终可用空间 687 MiB。run、main、evidence 和
  corruption 目录权限为 0700，spool 文件为 0600。`/tmp` 是 tmpfs，只用于 binary/
  library 部署，没有被当作耐久性证据。
- 专用 `device_id` 为 `m7-gateway-20260901-105414`，topic 为
  `test/m7/20260901T105414/recovery`，spool 从全新文件开始。

## 断线、SIGKILL 与恢复

1. 在线 phase1 收到 batch 1～19、gateway seq 1～4864；Broker 记录 19 次 gateway
   PUBLISH 和 19 次 PUBACK，subscriber 同样完成 19 次 QoS 1 交付确认。
2. 实际停止 Broker 后 gateway PID 1706 继续运行。外部 CAN 源暂停并排空内存队列后，
   spool 为 34309 条、2744720 bytes；cursor 仍停在 offset 389120、seq 4864、next
   batch 20，因此有 29445 条已持久化但未确认记录。离线稳定窗口内 state hash 不变。
3. 对核实了 comm、cmdline 和本 run 配置的 PID 1706 只执行一次 `kill -9`。命令返回 0，
   wrapper `wait_exit=137`；前后 spool 大小、data SHA256
   `0465c82da8da6bcbdd4b7cba44361197279fe3519c8d1a37a8a897eba241bbbf`
   和 state SHA256
   `59d3246e7632a423ed3fbb9aa53d42fa0ddf2e7f63b8519927d7cc1e1f6a0d97`
   均不变。
4. 使用同一 binary、配置和 spool 启动 PID 1926。Broker 恢复后 cursor 从 seq 4864
   单调推进到 34309，subscriber 收到 seq 4865～34309，共 29445 条，无缺失、重复或
   seq 复用。
5. backlog 清零后再次停止 Broker，PID 1926 记录运行中连接丢失且继续存活；恢复 Broker
   后同一 PID 重连。随后恢复物理 CAN 发送窗口并再次暂停，新增 1335 条最终全部 ACK，
   cursor 到 offset 2851520、seq 35644、next batch 142。
6. phase2 正常退出 summary：publish accepted/PUBACK 为 122/122，unexpected PUBACK 0，
   reconnect 1，spool append/replay/ACK/pending 为 1335/30780/30780/0，queue drop 0，
   spool corruption/error 为 0/0。

## 损坏恢复

- 尾部副本：在 offset 2851520 追加 `de ad be ef`。文件从 2851520 增至 2851524 bytes，
  SHA256 从 `3c5f8309...0384bca` 变为 `a7725ee0...149009e`；启动后仅截断这 4 bytes，
  size 和 SHA256 精确恢复为原有效前缀，`spool_tail_recoveries=1`，state 未改变。
- 内部副本：在 offset 80000 将 entry 1000 的首 magic byte 从 `47` 改为 `00`；size
  不变，SHA256 变为 `82a655e9...5a76861`。gateway exit 1，在 MQTT connect 前拒绝
  启动，损坏 data/state 未被越过或改写。
- state/cursor：在主测试 state offset 0 将 magic `47` 改为 `00`；data SHA256 不变。
  启动后 `spool_state_recoveries=1`，从 offset 0 安全重放 35644 条，140/140 publish/
  PUBACK，最终 pending 0。原始重复被保留，未错误跳过未确认数据。

## 最终 subscriber 与 Broker 对账

严格 validator 命令使用本 run 的 `combined.jsonl`、专用 device ID、期望 seq 1～35644
和 `--require-raw-duplicates`；其自测 5/5、实际捕获验证均 exit 0：

| 指标 | 实测 |
| --- | ---: |
| raw batches | 281 |
| raw records | 71288 |
| raw duplicate records | 35644 |
| unique batch seq | 141 |
| unique gateway seq | 35644 |
| first/last gateway seq | 1 / 35644 |
| missing gateway seq | 0 |
| effective duplicate records | 0 |

Broker 原始日志独立计数如下：phase1 19/19，phase2 初次恢复 116/116，重连 Broker
146/146，总计 281 次 gateway PUBLISH / 281 次 PUBACK；Broker 向 subscriber 的 PUBLISH
及 subscriber 返回 PUBACK 也是 281/281。该计数与 `raw_batches=281` 差值为 0。

gateway 侧 phase2、phase3 正常 summary 分别为 122/122 和 140/140 publish/PUBACK；
phase1 因门禁要求的 SIGKILL 没有最终 summary，其 19 次已确认 batch 由 Broker 原始日志
及落盘 cursor 共同证明。因此 gateway/Broker/subscriber 三侧总计均为 281 个原始 batch。

## queue、CAN 与限制

- phase2 和 phase3 的正常退出 summary 均为 `queue_drop=0`；尾部恢复副本也是 0。
- phase1 按要求以 SIGKILL 结束，无法取得该进程的最终 `queue_drop` summary，故该值为
  `NOT AVAILABLE`，不能填成 0。phase1 日志没有 queue-drop warning，最终 gateway_seq
  验证为 1～35644 连续，但这只证明已分配 gateway_seq 的数据无缺失。
- `can0` 起始为 UP、500 kbit/s、ERROR-ACTIVE、berr tx/rx=0/0；最终为 UP、
  ERROR-WARNING、berr tx/rx=0/102。Linux RX errors/dropped 仍为 0/0；累计 error-warn
  从 220 增至 223，error-pass 保持 3，bus-off 保持 0。该变化如实保留，不声称本次物理
  CAN 错误计数为零。
- 目标 wall clock 未设置，板端日志显示 1970。正确 UTC、端到端时延、吞吐、CPU/RSS、
  长期稳定性、真实断电恢复、存储掉电语义、介质寿命、容量阈值和 compaction：
  `NOT RUN`。
- LeakSanitizer：`NOT RUN`；既有 M7 离线证据仅为关闭 leak 检测后的 ASan+UBSan 16/16。

## 证据完整性与失败历史

- 板端 BusyBox `tar` 不支持 `-z`，第一次压缩导出失败并保留；第二次使用未压缩 tar
  成功。归档 11585024 bytes，SHA256
  `51bd1b5c4197c2703ebd10478bd9b39b2ac9b38b991046e95ce5e0dd2f30ae4b`。
- 本地在解包前检查所有 124 个 tar 条目，无绝对路径或 `..`；板端 manifest 115/115
  在本地逐文件重算通过。
- 原始含敏感地址的 Broker、板端归档和配置保存在 Git 忽略的 `private_raw/`；
  `raw_private_sha256.txt` 绑定 27 个未提交原始文件，可提交目录仅保存完整脱敏日志、
  subscriber JSON、命令、统计和 hash。
- `20260901T100727+0800-m7-lan-recovery-final` 因 `queue_drop=1303` 且 subscriber
  missing=309 失败；`20260901T103725+0800-m7-lan-recovery-gate` 因 `queue_drop=72`
  中止。两次均正常停止且没有执行 SIGKILL；证据原样保留，不参与本次 PASS 判定。
- 本 run 两个“等待离线日志”脚本分别在 3 秒和 15 秒时误判失败；实际 TCP connect
  timeout 约 134 秒，之后同一 PID 记录了离线状态。误判记录和后续纠正证据均保留，
  没有重复执行 SIGKILL。

本结论只关闭 M7。没有批准或进入 M8。
