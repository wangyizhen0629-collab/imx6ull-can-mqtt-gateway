# 简历事实可追溯性

当前没有任何一条网关功能结论可以写入简历。M0 只证明主机骨架构建和证据流程，
没有证明 ARM、硬件或网关功能；数值字段在 M10 真实测量前保持空缺。

| 候选描述 | 必需源码/配置 | 必需测试 | 必需证据 | 真实值 | 可写简历 |
| --- | --- | --- | --- | --- | --- |
| SocketCAN 接收/过滤/内核时间戳和自定义 DBC 解码 | M2/M4 源码和协议文件 | loopback、过滤、时间戳、黄金/实时报文解码 | M2、M3-D/E、M4 run | 未测量 | 否 |
| STM32 确定性模拟 ECU 提供真实物理 CAN 输入 | M3-A/B `.ioc`、Keil 工程和业务源码 | Keil Build、断电终端测量、`candump`、周期/计数器 | M3-A～M3-D Windows/硬件 run | 未测量 | 否 |
| pthread 有界生产者--消费者队列及明确过载策略 | M1/M5 队列、生命周期、stats 配置 | 并发/满队列/close 单测和目标基准/过载 | M1 主机和 M5 板端 run | 未测量 | 否 |
| MQTT QoS 1、seq、PUBACK、本地 spool、重连补传 | M6/M7 源码和 spool 格式/配置 | 1000 batch、断线、损坏、崩溃恢复 | M6/M7 集成 run | 未测量 | 否 |
| epoll 统一 eventfd/timerfd/MQTT socket | M8 reactor 和 API 兼容性记录 | 与 M7 等价的 reactor/重连/退出测试 | M8 目标 run | 未测量 | 否 |
| BusyBox 开机启动和异常退出恢复 | M9 init/supervisor/config | 启动和受控 crash/restart | M9 板端 run | 未测量 | 否 |
| 压力、重复断网、CPU/RSS 和 24 小时稳定性 | M10 工具和精确配置 | 经批准的压力/断网/稳定性流程 | M10 报告 | 未测量 | 否 |

一行满足条件后，只能用已有不可变 run 计算出的数值替换“未测量”，并补充精确源码、
测试、配置和 `artifacts/<run_id>/` 链接。STM32 Keil 结果与 Linux 结果必须分别标明环境。
