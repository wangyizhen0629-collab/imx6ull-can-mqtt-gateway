# M6 收尾审计

- 审计时间：2026-08-31T14:08:38+08:00
- 起点提交：`ae4fd3fdff908eab5112a065e6e3cc23d88f8284`
- 审计结论：**PASS（证据与M6范围一致性）**
- M6总退出门禁：**NOT MET**

## 实际执行

1. 在既有warning-clean最终构建上重跑 `ctest -L M6`：1/1 PASS。这是
   `test_mqtt_sink` 单元测试，没有启动Broker。
2. 对最终loopback run保存的1000条subscriber JSON重放validator：PASS；
   batch_seq和gateway seq都是1～1000，missing、duplicate、reordered均为0。
3. 根据集成run归档的 `source_sha256.txt` 复核18个M6源码/测试文件：
   全部PASS。根据 `binary_sha256.txt` 复核publisher、timing和 `gatewayd`三个
   x86_64二进制：全部PASS。
4. 解析9个M6 manifest/summary JSON：PASS。Python validator语法编译：PASS，
   pycache写入 `/tmp`，没有污染仓库。
5. `git diff --check`：PASS。范围搜索没有找到断线重连、libmosquitto后台
   loop thread、spool I/O或epoll实现，确认未越界到M7/M8。
6. 沙箱内 `ss` 因netlink权限不能完成端口复核；随后以获批准的只读
   非沙箱命令复核 `127.0.0.1:18884`，只有表头、无listener，确认临时Broker
   已停止。本审计没有重新启动Broker。

## 命令修正记录

首次validator重放命令误加了脚本不支持的 `--expected-first-seq` 和
`--expected-last-seq` 参数，因此命令行解析退出2；该原始失败日志保留为
`validator_replay.log`。改为脚本真实支持的参数后，`validator_replay_pass.log`
记录PASS。这是收尾命令输入错误，不是payload或实现失败，两份结果均未覆盖。

## NOT RUN和门禁

本审计没有新的目标侧条件，因此以下项目继续为 `NOT RUN`：

- ARMv7 M6交叉构建（Buildroot SDK缺少目标libmosquitto开发文件）；
- i.MX6ULL部署、动态加载和板端运行；
- 真实物理CAN → decode → queue → MQTT端到端链路；
- 局域网跨主机Broker/subscriber门禁；
- 硬件、性能、时延、持续运行和可靠性测试。

因 `docs/TEST_PLAN.md` 规定的局域网测试以及目标端构建/运行证据仍然缺失，
M6总门禁保持 `NOT MET`。本次收尾没有实现或测试M7及后续功能。
