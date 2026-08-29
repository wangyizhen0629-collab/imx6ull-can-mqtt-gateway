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

M1、M2 已完成。M2 的有限 SocketCAN 路径通过主机回归、ARMv7 交叉构建和真实 i.MX6ULL
controller loopback；证据覆盖目标/非目标 ID、错误 DLC 和内核时间戳。该结果不是物理
CAN、DBC 或性能证据。当前没有实现真实生产者--消费者数据链路、DBC、MQTT、spool
I/O、epoll 或 STM32 固件；M3 仍未开始，进入下一阶段需要另行确认。

## Ubuntu 主机构建

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
./build/gateway/gatewayd
./build/gateway/gatewayd --config gateway/config/gateway.conf.example
./build/gateway/gatewayd --config gateway/config/gateway.conf.example \
  --set queue_capacity=128 --print-config
```

`--can-receive COUNT --can-timeout-ms MS` 是 M2 板端有限验证入口，不会自动修改 CAN
接口状态。使用它之前必须按仓库规则获得修改目标板 `can0` 和启动测试进程的明确批准；
流程见 [tools/can/README.md](tools/can/README.md)。

请先阅读[项目规范](docs/PROJECT_SPEC.md)、[阶段计划](docs/PLANS.md)和
[待确认问题](docs/OPEN_QUESTIONS.md)。M0 证据记录在
[docs/milestones/M0.md](docs/milestones/M0.md)，M1 证据记录在
[docs/milestones/M1.md](docs/milestones/M1.md)，M2 完成记录在
[docs/milestones/M2.md](docs/milestones/M2.md)。
