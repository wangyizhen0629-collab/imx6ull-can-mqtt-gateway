# M1 最终主机测试报告

结果：**PASS**。Ubuntu 22.04.5 x86_64 上使用 GCC 11.4.0 完成 warning-clean Debug
构建，CTest 8/8 PASS。测试覆盖配置/覆盖/边界/脱敏、错误和 64 字节记录布局、日志、
signal 生命周期、FIFO、满队列 timeout、close/broadcast、stats 和多线程不变量。

直接配置运行退出 0，`queue_capacity` 从文件的 1024 被 CLI 覆盖为 8；测试假密码未
出现在日志，且 `broker_password=<redacted>` 存在。

本 run 只证明 x86_64 主机功能。ARMv7、i.MX6ULL、SocketCAN、真实 CAN、STM32、DBC、
实际数据链路、MQTT、spool、部署、性能和长时间测试均为 **NOT RUN**。
