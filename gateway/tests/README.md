# Gateway 测试

M1 测试不依赖外部测试框架，由 CMake/CTest 注册以下主机测试：

- 配置默认值、文件加载、命令行覆盖、未知/重复/非法/边界值和日志凭据脱敏；
- 稳定错误码、`telemetry_record` 的 64 字节大小与关键字段偏移；
- 日志等级过滤、UTC 时间戳和基本格式；
- `SIGINT`/`SIGTERM` 自管道通知、条件变量退出唤醒和超时；
- ring buffer FIFO、满队列有界超时、close 后 drain、broadcast 唤醒、并发唯一性和
  stats/high-watermark 不变量。

这些测试只证明当前 Ubuntu x86_64 主机实现，不证明 ARMv7、SocketCAN、真实 CAN、
MQTT 或板端性能。
