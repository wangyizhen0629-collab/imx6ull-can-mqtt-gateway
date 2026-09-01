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

M4 新增 `test_vehicle_decoder` 和 `test_vehicle_dbc`：

- C 解码器读取 `protocol/test_vectors/vehicle_golden.csv`，验证三类消息、Intel 小端、
  定点缩放/偏移、bit mask、counter/checksum、信号边界及错误清零语义；
- 对当前 STM32 的完整60秒整数定点语义场景执行静态解码，共验证6660帧；
- Python 标准库检查器独立读取同一黄金向量和 `vehicle.dbc`，验证 DBC 位提取、物理量
  缩放及 XOR 规则，避免 C 实现和协议文件各自维护互不相干的期望值。

当前42条向量中31条取自实物捕获，另含边界和错误路径。物理原始证据由
`tools/protocol/check_stm32_candump.py` 独立逐帧复核。这些测试不会连接或修改真实
`can0`；即使输入来自实物，也不能把离线审计描述成 M4 解码器已在 i.MX6ULL 实时运行。

M5 新增 `test_pipeline`：

- 以单调时钟定时生成100/10/1 Hz组成的111条合成 CAN 记录，验证 receive、DBC decode、
  queue 和 mock sink 全链路 queue drop 为0；
- 以 capacity 4、push timeout 0和2 ms慢消费者制造过载，验证丢弃新记录以及
  push/pop/consumed/high-watermark/close-drain 不变量；
- 注入 checksum 错误，验证错误记录不入队且 seq gap 可观察；
- 使用真实 `SIGTERM` 和 self-pipe，验证 producer/consumer 唤醒、drain 和 join。

这些用例本身仍是 Ubuntu 主机功能测试。真实 i.MX6ULL 物理 CAN 基准、板端过载和
signal 15 退出已由项目所有者另行执行，原始日志归档在
`artifacts/20260831T132341+0800-m5-board-owner-final/`。该板端 run 只证明 M5 功能
门禁，不提供持续111帧/s、CAN 错误增量、吞吐、时延或可靠性结论。

M6新增 `test_mqtt_sink`，默认CTest只做无需Broker的batch JSON编码、错误边界、实际
libmosquitto加载和配置检查。`test_mqtt_integration` 与 `test_mqtt_timing` 会连接并改变
Broker/进程状态，所以只构建、不注册到默认CTest；必须先获批准，再显式启动测试Broker
和subscriber后运行。

M6最终host loopback集成使用实际Mosquitto/libmosquitto 2.0.11：1000个单记录QoS 1
batch全部收到匹配PUBACK，subscriber的batch_seq和gateway seq均为1～1000且无缺失、
重复或乱序。独立100 ms测试interval用例验证低流量idle tick能发送到期batch。证据在
`artifacts/20260831T135630+0800-m6-mqtt-final/`。

上述段落记录M6 host loopback时的边界；后续真实i.MX6ULL到Windows Broker证据已经
关闭M6，详见`docs/milestones/M6.md`。

M7新增`test_spool`和`test_mqtt_recovery_validator`并注册到默认CTest：

- `test_spool`覆盖固定entry/state格式、CRC、append同步、PUBACK游标等价API、重开、
  部分尾部截断、state损坏安全回退、内部损坏拒绝及单写者锁；
- Python恢复validator允许内容一致的raw duplicate，要求按`device_id + seq`去重后连续
  完整，并拒绝缺失、首次乱序或内容冲突的重复；
- `test_mqtt_sink`增加无需Broker的offline durable append和gateway seq重启恢复覆盖。

`test_mqtt_recovery_driver`只构建、不注册到默认CTest；
`tools/mqtt/run_m7_recovery_integration.py`会实际启停loopback Broker/subscriber并发送
`SIGKILL`，只有获得明确批准后才能显式运行。M7最终门禁使用真实Windows Broker/板端
证据而不是该脚本；M8将该驱动扩展为reactor计数断言，以验证external-loop保持M7行为。
当前M8本机跨进程回归因批准未取得为`NOT RUN`，不能用离线单测替代。

M8新增`test_mqtt_reactor`，离线覆盖epoll/eventfd/timerfd创建、wake、timer和client
rebind；`test_mqtt_integration`、timing和恢复驱动新增实际socket read/write计数门禁。
真正的MQTT socket路径仍必须显式启动Broker后验证。
