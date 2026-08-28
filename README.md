# i.MX6ULL CAN--MQTT 网关原型

这是一个按阶段门禁推进的实验型 CAN--MQTT 网关项目。目标链路为：
STM32F103C8T6 确定性模拟 ECU -> 500 kbit/s 物理 Classic CAN -> i.MX6ULL
SocketCAN -> 自定义 DBC 解码 -> 有界队列 -> 带持久化 spool 的 MQTT QoS 1 ->
PC 端 Mosquitto validator。

协议是自定义确定性测试协议，不是 OEM 或量产车辆协议。

## 开发环境

- Windows：STM32CubeMX、Keil MDK、ST-Link 和 STM32 源码/Git 工作。
- Ubuntu 虚拟机：`gatewayd`、ARM 交叉编译、i.MX6ULL 上板和 Linux 侧测试。
- 两边使用同一 Git 仓库的不同 clone，仓库源码是唯一可信源。

## 当前状态

M0 已完成；当前没有活动 Milestone，等待用户确认是否启动 M1。`gatewayd` 仍只是
主机生命周期/配置文件可读性骨架，尚未实现 SocketCAN、MQTT、ring buffer、spool、
epoll 或 STM32 固件。

## Ubuntu 主机构建

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
./build/gateway/gatewayd
./build/gateway/gatewayd --config gateway/config/gateway.conf.example
```

请先阅读[项目规范](docs/PROJECT_SPEC.md)、[阶段计划](docs/PLANS.md)和
[待确认问题](docs/OPEN_QUESTIONS.md)。M0 证据记录在
[docs/milestones/M0.md](docs/milestones/M0.md)。
