# M5 Ubuntu 主机最终测试

结论：主机范围 `PASS`。GCC 11.4.0 严格警告构建无错误，全量 CTest 12/12 PASS，
其中 M5 `test_pipeline` PASS。直接运行专项测试记录了以下真实结果：

- 定时合成源按100/10/1 Hz组成在约1秒内产生111帧；111帧全部解码、入队并由 mock
  sink 消费，queue drop 为0，high-watermark 为2；
- 容量4、零等待入队、2 ms慢消费者的故意过载产生200条输入，其中5条入队/消费、
  195条按策略丢弃新记录；high-watermark 为4，所有 queue 计数不变量成立；
- `SIGTERM` 15 经真实 signal handler/self-pipe 传递后，producer 和 consumer 均退出，
  队列关闭并排空。

这些结果只证明 Ubuntu x86_64 上的 M5 功能和过载策略，不是 i.MX6ULL 性能或实时物理
CAN 证据。当前会话未获目标板连接，也未获部署、修改 `can0` 或启动/终止板端进程的
独立批准，因此三项目标板 M5 测试均为 `NOT RUN`。
