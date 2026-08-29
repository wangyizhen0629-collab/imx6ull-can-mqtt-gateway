# CAN 工具与 M2 板端流程

`gatewayd --can-receive COUNT --can-timeout-ms MS` 只打开配置中的 `can_interface`，安装
`0x100`/`0x101`/`0x102` 精确标准数据帧过滤器并接收有限数量的 DLC-8 帧。它不会执行
`ip link set`，也不会发送报文。该入口只用于 M2 门禁，不会入队、解码或连接 MQTT。

M2 已于 2026-08-29 使用 `run_m2_board_loopback.sh` 在真实 i.MX6ULL 完成本流程，原始
证据为 `artifacts/20260829T133148+0800-m2-board-loopback/`，最终审计为
`artifacts/20260829T134148+0800-m2-final-audit/`，均按报告判定 PASS。该历史 run 不得
复用；本页其余步骤保留为流程说明，不表示允许自动重跑或进入物理 CAN/M3。

## 执行前条件

下列条件缺一时必须写 `NOT RUN`：

1. 已用真实 Buildroot SDK/sysroot 完成 ARM 交叉编译，并记录 compiler/ABI/sysroot。
2. 已确认测试的是 i.MX6ULL，而不是 Ubuntu 主机或虚拟 `vcan`。
3. 板端存在 `ip`、`cansend` 和待测 `can0`，且测试二进制/配置已部署到非生产目录。
4. 已获得本次修改 `can0` 状态、启动进程和发送 loopback 流量的明确用户批准。
5. 已创建此前不存在的唯一 `artifacts/<run_id>/`，并先保存 Git、主机、板端和
   `can_before.txt` 信息；禁止复用或清空旧目录。

## 经批准后的命令骨架

以下只是待执行步骤，仓库创建 M2 源码时没有运行这些命令。真实操作前还必须依据
`can_before.txt` 确定测试结束后的恢复方式，禁止猜测原配置。

```sh
ip -details -statistics link show can0
ip link set can0 down
ip link set can0 type can bitrate 500000 loopback on
ip link set can0 up
```

目标 ID 与时间戳用例：先后台启动接收 3 帧，再交错发送目标和非目标 ID；日志中必须
只有三条 `M2_CAN_FRAME`，ID 依次为 `0x100`、`0x101`、`0x102`，每条都含正数
`kernel_timestamp_ns`，summary 为 `accepted=3`、`rejected_*=0`、`timeouts=0`。

```sh
gatewayd --config gateway.conf --can-receive 3 --can-timeout-ms 5000
cansend can0 123#1122334455667788
cansend can0 100#0102030405060708
cansend can0 124#1122334455667788
cansend can0 101#1112131415161718
cansend can0 125#1122334455667788
cansend can0 102#2122232425262728
```

命令骨架中的 `gatewayd` 必须由测试控制 shell 放到后台并在发送后 `wait`，同时保存它的
真实退出码；上面分行展示是为了避免把未经批准的进程控制包装进自动脚本。

非目标过滤用例应单独启动 `--can-receive 1 --can-timeout-ms 1500`，只发送 `0x123`，
预期 `gatewayd` 因 timeout 非零退出、`accepted=0` 且没有 `M2_CAN_FRAME`。DLC 用例也
单独启动相同接收命令，只发送 `100#010203`，预期日志包含 `reason=dlc`，summary 为
`rejected_dlc=1`、`accepted=0`、`timeouts=1`。这两个“预期非零”必须由控制脚本显式
判定，不能把非零退出直接误记为测试失败。

短于/长于 `struct can_frame` 的 SocketCAN datagram 不能由正常 `cansend`/CAN 控制器
产生，使用主机 `test_can_receiver` 的 datagram socket 注入覆盖；板端只验证错误 DLC。
所有用例完成后保存 `can_after.txt`，按已核实的原状态恢复接口，再次保存恢复结果。本次
只读审计中的原状态是 DOWN/STOPPED，且没有已配置 bitrate。Linux `ip link` 没有通用的
“清空 CAN bit timing”操作；测试后可安全执行 `ip link set can0 down` 并在 down 状态将
`loopback off`，但 500000 bit/s 参数可能仍显示在接口配置中。不得把这种结果写成“完全
恢复原配置”。若必须清除 bit timing，需要另行批准并验证重启或驱动重新绑定方案；M2
默认不做这类扩大范围的操作。
