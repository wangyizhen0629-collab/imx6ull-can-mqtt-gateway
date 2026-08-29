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

M2 新增 `test_can_receiver`，覆盖：

- 未绑定 `CAN_RAW` socket 上的三条精确过滤器及 `SO_TIMESTAMPNS` 选项；
- 通过 datagram socket 的真实内核辅助时间戳提取和记录字段填充；
- 非目标 ID、扩展帧、RTR、错误 DLC、短/长 datagram、缺失时间戳和接收超时。

主机内核/sandbox 允许创建 `PF_CAN` socket 时，该测试可以证明 socket 选项配置和纯接收
逻辑；它不能证明目标接口 bind、i.MX6ULL 驱动或 controller loopback。后者必须按
`tools/can/README.md` 在真实板端另建 run。

M2 的独立真实板端 run 已保存在
`artifacts/20260829T133148+0800-m2-board-loopback/`，最终证据审计在
`artifacts/20260829T134148+0800-m2-final-audit/`。两者证明 controller loopback，不把
主机测试或 loopback 表述为物理 CAN、STM32 或性能结果。
