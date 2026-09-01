# M8 external-loop recovery integration

状态：**NOT RUN**。

计划使用仅绑定 `127.0.0.1:21885`、禁用持久化的临时 Mosquitto，运行现有 M7 恢复
驱动，在 M8 reactor 下重新验证在线发布、Broker 离线后重连、持久 spool 补传、一次
专用驱动 `SIGKILL`、同 spool 重启以及 state 损坏安全重放。审批因尚缺项目所有者在
知情后的明确授权而被拒绝；没有启动任何 Broker/subscriber/driver，没有发送信号，
也没有占用端口。

所需条件：项目所有者明确批准上述仅限本机 loopback 的临时 Broker/进程/SIGKILL
测试。该批准不包含目标板、Windows Broker、M9 或任何长时间测试。
