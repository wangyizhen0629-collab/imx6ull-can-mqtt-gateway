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
离线实物日志审计也不等于新增解码器已在 i.MX6ULL 实时运行。M4 已关闭。

M5 已实现 CAN receive → checksum/DBC decode → 有界队列 → mock sink 的单 producer/
consumer 链路，并完成 Ubuntu warning-clean/ASan+UBSan 全量12/12、主机111帧/s零
queue drop、故意慢消费者、`SIGTERM` 和 ARMv7 交叉构建。SHA256 固定的 ARM binary
随后在真实 i.MX6ULL 物理 CAN 上运行：基准窗口3694条全部解码/入队/消费且 queue drop
为0；慢消费者过载产生3561次明确 drop，计数守恒；两次 signal 15 后均完成 post-join
summary。M5 已通过。

M6 已完成 `libmosquitto` MQTT 3.1.1 QoS 1 sink、有界批量JSON、定时刷新及匹配
PUBACK门禁。真实i.MX6ULL使用物理CAN输入连接Windows Broker；正式subscriber保存
1000批/115335条连续记录，gateway/Broker对账1033次publish/PUBACK一致，独立manifest
与原始证据复核通过。M6状态为`MET`，但不产生正确UTC、性能或可靠性结论。

M7 已实现固定格式append-only spool、CRC、原子PUBACK cursor、尾部/state恢复、gateway
seq恢复、断线重连、单写者锁和去重validator。Ubuntu普通/ASan+UBSan全量16/16及ARMv7
交叉构建通过；真实i.MX6ULL ext4 spool、Windows Broker断线、一次`kill -9`、补传及
损坏恢复均有证据，M7门禁为`MET`。M8随后确认目标libmosquitto external-loop API并
实现epoll/eventfd/timerfd reactor；离线/ARM和真实Broker等价恢复均通过，M8为`MET`。
M9已实现BusyBox inittab/前台supervisor、受控restart、异常重拉和风暴冷却，主机全量
CTest/ASan+UBSan及ARM构建通过。Windows续跑已只读确认真实目标PID 1链接BusyBox
1.31.1，但指定M9 ARM binary未能从Ubuntu认证转交，Windows和目标板也没有预期SHA
文件；因此未部署或修改`/etc`，开机、restart、异常恢复和cooldown仍为`NOT RUN`。
M9总门禁为`NOT MET`，M10尚未开始。

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

M6 构建需要 `libmosquitto` 头文件和链接库。它们不在系统默认路径时，可以在配置
阶段指定一个已展开的依赖根目录：

```sh
cmake -S . -B build -DGATEWAY_MOSQUITTO_ROOT=/path/to/mosquitto/root
```

`--can-receive COUNT --can-timeout-ms MS` 是 M2 板端有限验证入口，不会自动修改 CAN
接口状态。使用它之前必须按仓库规则获得修改目标板 `can0` 和启动测试进程的明确批准；
流程见 [tools/can/README.md](tools/can/README.md)。

`--run-mock-sink [--mock-sink-delay-ms MS]` 是 M5 实时链路入口。它不会自行配置
`can0`，但会打开配置的 CAN 接口并启动工作线程；在目标板部署/运行、修改接口状态或
发送进程信号前仍必须取得明确批准。

`--run-mqtt` 是当前M8实时链路入口，它同样不会自行配置`can0`或启动Broker，但会打开
CAN接口、创建/锁定`spool_path`、连接配置的Broker并启动工作线程。目标板部署/运行、
Broker状态改动、进程kill或网络/CAN状态改动仍受仓库批准规则约束。

请先阅读[项目规范](docs/PROJECT_SPEC.md)、[阶段计划](docs/PLANS.md)和
[待确认问题](docs/OPEN_QUESTIONS.md)。M0 证据记录在
[docs/milestones/M0.md](docs/milestones/M0.md)，M1 证据记录在
[docs/milestones/M1.md](docs/milestones/M1.md)，M2 完成记录在
[docs/milestones/M2.md](docs/milestones/M2.md)，M3/M4/M5 记录分别在
[docs/milestones/M3.md](docs/milestones/M3.md)和
[docs/milestones/M4.md](docs/milestones/M4.md)、
[docs/milestones/M5.md](docs/milestones/M5.md)，M6 执行记录在
[docs/milestones/M6.md](docs/milestones/M6.md)，M7完成记录和M8当前记录分别在
[docs/milestones/M7.md](docs/milestones/M7.md)、
[docs/milestones/M8.md](docs/milestones/M8.md)，M9当前记录在
[docs/milestones/M9.md](docs/milestones/M9.md)。
