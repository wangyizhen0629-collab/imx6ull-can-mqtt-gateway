# M6 Windows Broker 本地 smoke test 报告

- Run ID：`20260831T172756+0800-m6-windows-broker`
- 仓库提交：`03afe1f200d5a8a39d4a30ab8ea733a6e1dbc9e8`
- 结论：**PASS（仅 Windows 本地 Broker smoke test）**

## 前置条件

- Windows 有线网卡“以太网”处于 Up，IPv4 为 `192.168.50.1/24`，存在直连路由
  `192.168.50.0/24`。
- Windows 到 `192.168.50.2` 的 ping 为 4/4，0% 丢包。
- 防火墙规则恰好存在 1 条，已启用，方向 Inbound、动作 Allow；范围为 TCP、本地
  `192.168.50.1:18884`、远端 `192.168.50.2`。未关闭防火墙，也未创建重复规则。
- 测试前 TCP 18884 未被占用。
- 根据用户明确授权，直接使用现有 `E:\mosquitto`，没有下载或安装软件。

## Broker 配置和监听

配置精确绑定 `listener 18884 192.168.50.1`，并启用匿名访问、关闭持久化；匿名访问的
板端入口依赖上述防火墙规则限制远端地址。本次 Broker 以普通显式进程启动，PID 为
`31280`，没有使用 Windows 服务。

运行中 `Get-NetTCPConnection` 仅看到：

- LocalAddress：`192.168.50.1`
- LocalPort：`18884`
- State：`Listen`
- OwningProcess：`31280`

没有绑定 `127.0.0.1`、`0.0.0.0`、WLAN、VMnet1 或 VMnet8。

## 本地 QoS 1 smoke test

- Topic：`test/m6/windows-broker-smoke/20260831T172756p0800-m6-windows-broker`
- Payload：`{"schema":"m6.windows.broker.smoke","seq":1}`
- Publisher 退出码：`0`
- Subscriber PID：`25812`
- Subscriber 退出码：`0`
- Subscriber 输出：恰好 1 行；可解析为 JSON，字段和值与发布内容完全一致。
- 输入和订阅输出 SHA256 均为
  `B848CA752A000D85B803C01FC8E24FFFA1676E1D250B893ED631C619D7C9EA57`。
- Broker 日志记录了 2 次 client connect、1 次收到 PUBLISH、1 次向 publisher 发送
  PUBACK，以及 1 次收到 subscriber 的 PUBACK。
- `broker_stderr.log` 和 `smoke_subscriber.stderr` 均为空。

## 停止和环境边界

smoke test 完成后使用 `mosquitto_signal.exe -p 31280 shutdown` 正常停止临时 Broker。
最终检查显示 TCP 18884 监听数为 0，临时 Broker PID `31280` 和 subscriber PID
`25812` 均不存在，没有遗留本次测试进程。

系统中另有名为 `mosquitto` 的 Windows 服务，检查时为 Running / AUTO_START，PID
`33212`，命令行为 `"E:\mosquitto\mosquitto.exe" run`；该 PID 没有 TCP 监听。本次工作
没有创建、启动、停止或修改该服务，临时 Broker 也没有使用它。服务的测试前状态未被
采集，因此不能仅凭当前证据断言其何时启动。管理员 CIM 查询被拒绝，随后使用只读
`sc.exe queryex/qc` 取得上述事实。

## 来源限制和未运行项

`mosquitto.exe` 报告版本 2.1.2；`mosquitto_sub/pub` 报告可执行程序版本 2.1.2、运行于
libmosquitto 2.1.0。现有文件没有 Authenticode 签名；由于本次没有下载，官方安装包 URL、
安装包文件名及安装包 SHA256 为 **NOT RUN**，现有目录的官方来源无法独立验证。

以下内容均为 **NOT RUN**：i.MX6ULL 到 Windows 的 MQTT 数据通路、目标板 `gatewayd`、
1000-batch、目标 ARM 运行、硬件端到端、吞吐量或可靠性门禁。原因是本次授权仅限
Windows 本地 smoke test；这些检查需要另行批准并具备板端程序与对应采集条件。因此本
报告不能表述为完整 M6 门禁通过。本次没有修改 gateway 源码、validator、项目门禁文档，
也没有实施 M7 的重连、spool、恢复或去重功能。

## 保留的失败尝试

- `20260831T171523+0800-m6-windows-broker`：topic 将 `+` 放入层级导致订阅过滤器无效。
- `20260831T172228+0800-m6-windows-broker`：命令行 JSON 引号被 PowerShell 参数处理移除，
  subscriber 内容不是合法 JSON，且退出码采集不可靠。

两个目录均按不可覆盖原则保留并标记为 FAIL；本次 PASS 结论只对应本报告的 run ID。
