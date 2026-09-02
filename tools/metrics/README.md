# M10指标与门禁工具

`collect_proc_metrics.sh`兼容POSIX/BusyBox ash，每秒读取目标进程的
`/proc/<pid>/stat`、`/proc/<pid>/status`、`/proc/stat`和`/proc/uptime`。它支持固定PID
或PID文件，可选核对`/proc/<pid>/exe`，并把进程缺失、身份不符和读取错误写入CSV；输出
路径已存在时拒绝覆盖。示例：

```sh
sh tools/metrics/collect_proc_metrics.sh \
  --output /var/lib/gatewayd/m10-run/proc_metrics.csv \
  --pid-file /var/run/gatewayd/gatewayd.pid \
  --expected-exe /opt/gatewayd/bin/gatewayd \
  --samples 86401 --interval-sec 1
```

`report_proc_metrics.py`在Ubuntu侧检查CSV并生成JSON/Markdown。CPU采用相邻同一PID和
starttime的`utime+stime`增量除以`/proc/stat`总tick增量，属于“总系统CPU容量”口径；
VmRSS/VmHWM和CPU均报告平均、nearest-rank P95及最大值。
场景门禁还要求`/proc/uptime`覆盖完整测试时长、至少`duration+1`个样本，且任一相邻采样
间隔不得超过2.5秒，避免用稀疏快照冒充每秒采样。

`validate_m10_run.py`把上述CSV与`gateway.m10.run.v1`场景摘要组合校验。只接受
`environment=imx6ull-physical`，支持四个互不替代的唯一run：

- `stress_500`：500帧/s且至少1800秒；
- `stress_1000`：1000帧/s且至少1800秒；
- `broker_interruptions`：111帧/s、恰好20轮、每轮Broker离线至少300秒；
- `baseline_24h`：111帧/s且至少86400秒。

场景摘要还必须按`0x100/0x101/0x102`提供CAN帧数与counter gap，并提供CAN error frame、
RX error/drop/overrun增量、queue drop、MQTT batch/unique/missing/raw和effective
duplicate/reconnect、spool最大积压及进程退出数。总帧数必须达到`target_rate_fps *
duration_seconds`，三个ID计数之和必须等于总数，最终drain后的MQTT unique record也必须
与CAN总数相等。校验器要求gap/error/drop/missing/effective duplicate/exit均为0；QoS 1
raw duplicate允许存在。CPU/RSS当前只报告真实值，不虚构尚未冻结的产品阈值。

工具回归使用合成CSV和JSON，只证明解析、计算与拒绝规则，不是板端性能或稳定性证据。
