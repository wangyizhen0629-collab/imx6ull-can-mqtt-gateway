# M2 板端部署与前置状态核验

项目所有者通过 MobaXterm 在真实 100ASK i.MX6ULL 上执行命令并回传终端输出。
`/tmp/gatewayd-m2` 的 SHA256 与最终 ARM 交叉构建 artifact 完全一致，文件完整性核验
**PASS**。

操作前 `can0` 为 DOWN/STOPPED，RX/TX packet 和六类 CAN error 计数均为 0，输出中
没有已配置 bitrate。该状态可作为后续经批准 loopback run 的恢复基线。

本 run 没有启动 `gatewayd`、修改 `can0` 或发送 CAN 报文。板端动态加载、目标/非目标
ID、DLC 拒绝和内核 timestamp 均为 `NOT RUN`，M2 门禁仍未满足。
