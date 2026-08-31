# M5 主机开发验证

结论：`PASS`。Ubuntu GCC 11.4.0 严格警告构建成功，M5 专项测试通过。主机定时合成源
在约1秒内按100/10/1三类报文组成产生111帧，queue drop 为0且 mock sink 消费111帧。
容量4、零等待入队和2 ms慢消费者的故意过载用例产生196次 queue drop，仍满足
`push_success + push_timeout = attempts`、`pop_success = push_success = consumed`、
high-watermark 不超过容量且关闭后队列为空。真实 `SIGTERM` 经自管道触发，producer、
consumer 均完成退出。

这是开发阶段主机证据，不是最终全量回归、sanitizer、ARMv7 或真实 i.MX6ULL/物理 CAN
结果。后续改动不得把本 run 的时长当作板端时延或性能数字。
