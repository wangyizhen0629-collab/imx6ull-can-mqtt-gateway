# reload 后进程计数 attempt 1

结果：**FAIL（证据断言错误，服务未失败）**

PID 1 HUP exit 0 后，证据已显示一个 supervisor PID 11337 和一个真实 gatewayd 子 PID
11348；二者 10 秒内稳定，父子关系、cmdline、exe、binary/config/script SHA256 和
`/opt/gatewayd/lib` 映射均正确。

失败原因是首版计数把所有 `comm=gatewayd` 都当作 gatewayd binary。Linux 给以脚本名
启动的 shell supervisor 设置的 comm 也是 `gatewayd`，因此该方法将 supervisor 和真实
ELF 子进程合计为 2。`ps` 同时证明只有一条 supervisor 和一条 `/opt/.../gatewayd`。

后续 v2 只读审计必须以 cmdline 标识 supervisor，并以 exe 路径、SHA256、完整 cmdline
及 PPID 标识 gatewayd 子进程；不再使用 comm 单字段作唯一分类依据。
