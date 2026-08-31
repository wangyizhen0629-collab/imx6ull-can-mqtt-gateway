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

M1～M4 已完成。M3-A～M3-D 已由项目所有者按实际现象验收：STM32F103C8T6 使用
PA11/CAN_RX、PA12/CAN_TX，以 500 kbit/s 发送三类确定性报文，Keil Build 0 error、
0 warning；i.MX6ULL `candump` 已观察到正确 ID、周期、DLC、Rolling Counter 和 XOR。
`gatewayd` 又在真实 `can0` 完成两次1110帧短时接收，均为 accepted=1110 且接收错误
为0；项目所有者豁免原计划的10分钟 M3-E 门禁并接受 M3 完成。该状态不代表10分钟、
可靠性或性能验证。M4 已完成实验用自定义 DBC、整数定点静态 C 解码器和语义化 STM32
模拟车况闭环。当前42条黄金向量与完整60秒/6660帧主机模型通过，实物 `candump` 的
6000/600/60帧逐帧符合 DBC、独立 counter、XOR 和 spare-bit 规则，CAN 错误计数增量
为0。Keil 0 error/0 warning 及 Download 是项目所有者确认，完整原始控制台输出未归档；
离线实物日志审计也不等于新增解码器已在 i.MX6ULL 实时运行。M4 已关闭，进入 M5 仍需
项目所有者另行授权。
当前没有实现真实生产者--消费者数据链路、MQTT、spool I/O 或 epoll；M5 及后续未开始。

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
[docs/milestones/M2.md](docs/milestones/M2.md)，M3/M4 记录分别在
[docs/milestones/M3.md](docs/milestones/M3.md)和
[docs/milestones/M4.md](docs/milestones/M4.md)。
