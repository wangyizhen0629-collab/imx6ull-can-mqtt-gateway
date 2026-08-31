# M6 Windows Broker smoke（第二次失败保留）

结论：`FAIL`。临时 Broker PID 25264 精确监听 `192.168.50.1:18884`；安全 topic
订阅成功，publisher退出码为0，Broker原始日志包含CONNECT、QoS 1 PUBLISH、转发和
PUBACK，subscriber stderr为空。

但本流程没有从 `Start-Process -PassThru` 取得可用的subscriber退出码，不能满足
“subscriber exit=0”的明确门禁。并且 PowerShell 向原生 `-m` 参数传递JSON时双引号
被Windows参数解析移除，实际单行payload为 `{schema:m6.windows.broker.smoke,seq:1}`，
不是要求的JSON。因此本run不能判PASS。

Broker已通过 `mosquitto_signal shutdown` 停止，TCP 18884 listener数量为0。第三次
重试必须使用 `System.Diagnostics.Process` 直接取得subscriber退出码，并通过 `-f`
读取文件来逐字节发布JSON；不得覆盖本run。
